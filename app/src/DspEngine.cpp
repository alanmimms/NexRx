/**
 * @file DspEngine.cpp
 * @brief Implementation of IQ Signal Processing
 */

#include "DspEngine.hpp"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

DspEngine::DspEngine() {
  iqBuffer.assign(FFT_SIZE * 2, 0.0f);
  spectrumData.assign(FFT_SIZE, -100.0f);
  audioBuffer.configure(nexrx::BufferConfig{32768});
  demod.setSampleRate(96000.0f);
}

void DspEngine::setVfo(double freqHz) {
  lastVFOHz = freqHz;
  // Reset phasors and LMS on tuning to ensure stability
  w0_r = 0.0f; w0_i = 0.0f; 
  w1_r = 0.0f; w1_i = 0.0f;
  acc0_r = 0.0f; acc0_i = 0.0f;
  acc1_r = 0.0f; acc1_i = 0.0f;
  pwr0 = 0.0f; pwr1 = 0.0f;
  sampleBlockCounter = 0;
  totalSamplesProcessed = 0;
}

void DspEngine::setQsdOffset(double offsetKhz) {
  qsdOffsetKhz = offsetKhz;
}

std::vector<float> DspEngine::getSpectrumData() {
  std::lock_guard<std::mutex> l(spectrumMutex);
  return spectrumData;
}

#include <complex>

using Complex = std::complex<float>;

void DspEngine::processIQFrame(const nexrx::IQFrame& frame) {
  float i0, q0, i1, q1, i2, q2;
  frame.qsd[0].toFloat(i0, q0); 
  frame.qsd[1].toFloat(i1, q1); 
  frame.qsd[2].toFloat(i2, q2);
  
  // DC Offset Correction (running average subtraction)
  constexpr float dcAlpha = 0.0005f;
  dc0_i = (1.0f - dcAlpha) * dc0_i + dcAlpha * i0;
  dc0_q = (1.0f - dcAlpha) * dc0_q + dcAlpha * q0;
  dc1_i = (1.0f - dcAlpha) * dc1_i + dcAlpha * i1;
  dc1_q = (1.0f - dcAlpha) * dc1_q + dcAlpha * q1;
  
  i0 -= dc0_i; q0 -= dc0_q;
  i1 -= dc1_i; q1 -= dc1_q;

  constexpr double sampleRate = 96000.0;
  double k_hz = qsdOffsetKhz * 1000.0;
  double phase = 2.0 * M_PI * k_hz * (static_cast<double>(totalSamplesProcessed) / sampleRate);
  
  // High-precision phasors for rotation
  // Shift QSD0 DOWN by k: (i0 + j*q0) * (cos - j*sin)
  float i0_s = i0 * std::cos(phase) + q0 * std::sin(phase);
  float q0_s = q0 * std::cos(phase) - i0 * std::sin(phase);

  // Shift QSD1 UP by k: (i1 + j*q1) * (cos + j*sin)
  float i1_s = i1 * std::cos(phase) - q1 * std::sin(phase);
  float q1_s = q1 * std::cos(phase) + i1 * std::sin(phase);

  // Mixer 2 (6f) is the sextature mixer for harmonic rejection.
  // It is already at the target fundamental's perspective (center).
  Complex s2(i2, q2);

  // Historical LMS Adaptive Image Rejection
  // output = w0 * QSD0_shifted + w1 * QSD1_shifted ≈ QSD2
  Complex w0(w0_r, w0_i);
  Complex w1(w1_r, w1_i);
  Complex s0_s(i0_s, q0_s);
  Complex s1_s(i1_s, q1_s);

  Complex out = w0 * s0_s + w1 * s1_s;
  Complex err_vec = s2 - out;

  // LMS update: w = w + mu * error * conj(input)
  float mu = lmsMu.load();
  w0 += mu * err_vec * std::conj(s0_s);
  w1 += mu * err_vec * std::conj(s1_s);

  // Clamp weights to prevent runaway (converges to ~0.5 each in ideal)
  auto clampW = [](Complex& w) {
    float mag = std::abs(w);
    if (mag > 2.0f) w *= (2.0f / mag);
  };
  clampW(w0); clampW(w1);
  w0_r = w0.real(); w0_i = w0.imag();
  w1_r = w1.real(); w1_i = w1.imag();

  // The synthesized output is the filtered/nulled signal
  Complex err = out;

  if (++sampleBlockCounter >= 4093) {
    sampleBlockCounter = 0;
    // Store diagnostics
    dspDiag.lmsWeightR.store(std::abs(w0)); 
    dspDiag.lmsWeightI.store(std::abs(w1));
    
    // In this historical mode, we can estimate errors by deviation from 0.5
    dspDiag.gainErr0.store(20.0f * std::log10(std::max(0.1f, 2.0f * std::abs(w0))));
    dspDiag.phaseErr0.store(std::arg(w0) * 180.0f / M_PI);
    dspDiag.gainErr1.store(20.0f * std::log10(std::max(0.1f, 2.0f * std::abs(w1))));
    dspDiag.phaseErr1.store(std::arg(w1) * 180.0f / M_PI);
  }

  // Output after image suppression
  float iF = err.real();
  float qF = err.imag();
  
  float rfGain = std::pow(10.0f, rfGainDB.load() / 20.0f);
  iF *= rfGain; qF *= rfGain;
  float maxR = std::max(std::abs(iF), std::abs(qF));
  if (maxR > dspDiag.maxRaw.load()) dspDiag.maxRaw.store(maxR);

  basebandFilter.recompute(); 
  basebandFilter.process(iF, qF);
  
  size_t pos = iqBufferWritePos.load(std::memory_order_relaxed); 
  iqBuffer[pos*2] = iF; iqBuffer[pos*2+1] = qF;
  iqBufferWritePos.store((pos+1)%FFT_SIZE, std::memory_order_release);
  
  float aOut = demod.process(iF, qF);
  dspDiag.signalRms.store(demod.getSignalLevelRMS());
  if (std::abs(aOut) > dspDiag.maxAudio.load()) dspDiag.maxAudio.store(std::abs(aOut));
  
  if (!audioDecimateSkip) audioBuffer.write(aOut);
  audioDecimateSkip = !audioDecimateSkip;
  totalSamplesProcessed++;
  dspDiag.framesProcessed++;
}

void DspEngine::fftInPlace(float* re, float* im, size_t n) {
  for (size_t i=1, j=0; i<n; ++i) {
    size_t bit=n>>1;
    for (; j&bit; bit>>=1) j^=bit;
    j^=bit;
    if (i<j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); }
  }
  for (size_t len=2; len<=n; len<<=1) {
    float ang = -2.0f*M_PI/len;
    float wRe = std::cos(ang), wIm = std::sin(ang);
    for (size_t i=0; i<n; i+=len) {
      float cR = 1.0f, cI = 0.0f;
      for (size_t j=0; j<len/2; ++j) {
        size_t u = i+j, v = i+j+len/2;
        float tR = cR*re[v] - cI*im[v], tI = cR*im[v] + cI*re[v];
        re[v] = re[u]-tR; im[v] = im[u]-tI;
        re[u] += tR; im[u] += tI;
        float nR = cR*wRe - cI*wIm;
        cI = cR*wIm + cI*wRe; cR = nR;
      }
    }
  }
}

void DspEngine::computeSpectrum() {
  static std::vector<float> win;
  if (win.size() != FFT_SIZE) {
    win.resize(FFT_SIZE);
    for (size_t n=0; n<FFT_SIZE; ++n) win[n] = 0.5f*(1.0f-std::cos(2.0f*M_PI*n/(FFT_SIZE-1)));
  }
  static std::vector<float> fRe(FFT_SIZE), fIm(FFT_SIZE), avgS;
  static bool avgI = false;
  {
    std::lock_guard<std::mutex> l(spectrumMutex);
    if (iqBuffer.size() < FFT_SIZE*2) return;
    size_t wP = iqBufferWritePos.load(std::memory_order_acquire);
    for (size_t n=0; n<FFT_SIZE; ++n) {
      size_t idx = (wP+n)%FFT_SIZE;
      fRe[n] = iqBuffer[idx*2]*win[n];
      fIm[n] = iqBuffer[idx*2+1]*win[n];
    }
  }
  fftInPlace(fRe.data(), fIm.data(), FFT_SIZE);
  std::vector<float> lS(FFT_SIZE);
  for (size_t k=0; k<FFT_SIZE; ++k) {
    float mag = std::sqrt(fRe[k]*fRe[k] + fIm[k]*fIm[k])/FFT_SIZE*2.0f;
    lS[(k+FFT_SIZE/2)%FFT_SIZE] = (mag > 1e-10f) ? 20.0f*std::log10(mag) : -100.0f;
  }
  if (!avgI || avgS.size() != FFT_SIZE) { avgS = lS; avgI = true; } 
  else { for (size_t k=0; k<FFT_SIZE; ++k) avgS[k] = 0.3f*lS[k] + 0.7f*avgS[k]; }
  { std::lock_guard<std::mutex> l(spectrumMutex); spectrumData = avgS; }
}
