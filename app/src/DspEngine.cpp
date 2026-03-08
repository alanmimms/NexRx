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
  shiftCos0 = 1.0f; shiftSin0 = 0.0f; 
  shiftCos1 = 1.0f; shiftSin1 = 0.0f;
  lmsW0_r = 0.0f; lmsW0_i = 0.0f; 
  lmsAcc_r = 0.0f; lmsAcc_i = 0.0f;
  sampleBlockCounter = 0;
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
  
  constexpr float sampleRate = 96000.0f;
  float k_hz = static_cast<float>(qsdOffsetKhz * 1000.0);
  if (k_hz != lastShiftK) {
    float phaseInc = 2.0f * M_PI * k_hz / sampleRate;
    shiftCosD = std::cos(phaseInc); shiftSinD = std::sin(phaseInc);
    lastShiftK = k_hz;
  }

  // Advance complex LO phasors
  Complex d(shiftCosD, shiftSinD);
  Complex p0(shiftCos0, shiftSin0);
  Complex p1(shiftCos1, shiftSin1);
  
  p0 *= std::conj(d); // e^{-jkt}
  p1 *= d;           // e^{jkt}
  
  shiftCos0 = p0.real(); shiftSin0 = p0.imag();
  shiftCos1 = p1.real(); shiftSin1 = p1.imag();

  // Rotate mixers to DC
  // Mixer 0 (f-k) has signal at +k. Rotate by -k (p0)
  Complex s0(i0, q0);
  Complex s0_rot = s0 * p0;

  // Mixer 1 (f+k) has signal at -k. Rotate by +k (p1)
  Complex s1(i1, q1);
  Complex s1_rot = s1 * p1;

  // Periodically re-normalize phasors
  if ((dspDiag.framesProcessed.load() & 0x3FFF) == 0) {
    p0 /= std::abs(p0); p1 /= std::abs(p1);
    shiftCos0 = p0.real(); shiftSin0 = p0.imag();
    shiftCos1 = p1.real(); shiftSin1 = p1.imag();
  }

  // Signal combination (1-0-1)
  Complex sig = 0.5f * (s0_rot + s1_rot);
  Complex ref = 0.5f * (s0_rot - s1_rot);

  // LMS Adaptive Filter to null image components
  Complex w(lmsW0_r, lmsW0_i);
  // Error = Signal - W * conj(Ref)  -- null the image of the reference
  Complex err = sig - w * std::conj(ref);

  // Update LMS (deltaW = mu * error * Ref)
  Complex update = err * ref;
  lmsAcc_r += update.real();
  lmsAcc_i += update.imag();
  
  if (++sampleBlockCounter >= 32) {
    w += lmsMu * Complex(lmsAcc_r, lmsAcc_i);
    lmsW0_r = w.real(); lmsW0_i = w.imag();
    lmsAcc_r = 0.0f; lmsAcc_i = 0.0f; sampleBlockCounter = 0;
    dspDiag.lmsWeightR.store(lmsW0_r); dspDiag.lmsWeightI.store(lmsW0_i);
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
