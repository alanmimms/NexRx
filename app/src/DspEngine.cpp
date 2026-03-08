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
  wIQ0_r = 0; wIQ0_i = 0;
  wIQ1_r = 0; wIQ1_i = 0;
  wIQ2_r = 0; wIQ2_i = 0;
  wA0_r = 0.5f; wA0_i = 0;
  wA1_r = 0.5f; wA1_i = 0;
  accIQ0_r = 0; accIQ0_i = 0; pIQ0 = 0;
  accIQ1_r = 0; accIQ1_i = 0; pIQ1 = 0;
  accIQ2_r = 0; accIQ2_i = 0; pIQ2 = 0;
  accA0_r = 0; accA0_i = 0; pA0 = 0;
  accA1_r = 0; accA1_i = 0; pA1 = 0;
  sampleBlockCounter = 0;
  totalSamplesProcessed = 0;
}

void DspEngine::setCalibration(int ch, float gainDB, float phaseDeg) {
  if (ch < 0 || ch > 2) return;
  staticCal[ch] = {gainDB, phaseDeg};
  
  // Convert back to LMS weights immediately for live processing
  // w approx (1-G)/2 - j(P/2) where G is linear gain and P is phase in rad
  float g = std::pow(10.0f, -gainDB / 20.0f);
  float p = -phaseDeg * M_PI / 180.0f;
  
  if (ch == 0) { wIQ0_r = (1.0f - g)/2.0f; wIQ0_i = p/2.0f; }
  else if (ch == 1) { wIQ1_r = (1.0f - g)/2.0f; wIQ1_i = p/2.0f; }
  else if (ch == 2) { wIQ2_r = (1.0f - g)/2.0f; wIQ2_i = p/2.0f; }
}

void DspEngine::startManualCalibration() {
  std::cout << "[DSP] Starting manual calibration sequence..." << std::endl;
  // Reset LMS to defaults before training
  wIQ0_r = wIQ0_i = wIQ1_r = wIQ1_i = wIQ2_r = wIQ2_i = 0;
  wA0_r = wA1_r = 0.5f; wA0_i = wA1_i = 0;
  accIQ0_r = accIQ0_i = pIQ0 = 0;
  accIQ1_r = accIQ1_i = pIQ1 = 0;
  accIQ2_r = accIQ2_i = pIQ2 = 0;
  accA0_r = accA0_i = 0; accA1_r = accA1_i = 0;
  sampleBlockCounter = 0;
  calibrationActive.store(true);
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
  
  // DC Offset Correction
  constexpr float dcAlpha = 0.0005f;
  dc0_i = (1.0f - dcAlpha) * dc0_i + dcAlpha * i0;
  dc0_q = (1.0f - dcAlpha) * dc0_q + dcAlpha * q0;
  dc1_i = (1.0f - dcAlpha) * dc1_i + dcAlpha * i1;
  dc1_q = (1.0f - dcAlpha) * dc1_q + dcAlpha * q1;
  dc2_i = (1.0f - dcAlpha) * dc2_i + dcAlpha * i2;
  dc2_q = (1.0f - dcAlpha) * dc2_q + dcAlpha * q2;
  
  i0 -= dc0_i; q0 -= dc0_q;
  i1 -= dc1_i; q1 -= dc1_q;
  i2 -= dc2_i; q2 -= dc2_q;

  // 1. Independent Blind I/Q Correction for all 3 mixers
  // S_corr = S - w*conj(S)
  Complex s0(i0, q0), s1(i1, q1), s2(i2, q2);
  Complex wIQ0(wIQ0_r, wIQ0_i), wIQ1(wIQ1_r, wIQ1_i), wIQ2(wIQ2_r, wIQ2_i);
  
  Complex s0_c = s0 - wIQ0 * std::conj(s0);
  Complex s1_c = s1 - wIQ1 * std::conj(s1);
  Complex s2_c = s2 - wIQ2 * std::conj(s2);

  // 2. High-Precision Frequency Alignment
  constexpr double sampleRate = 96000.0;
  double k_hz = qsdOffsetKhz * 1000.0;
  double phase = 2.0 * M_PI * std::fmod(k_hz * (static_cast<double>(totalSamplesProcessed) / sampleRate), 1.0);
  
  // Shift QSD0 DOWN by k, QSD1 UP by k
  float cosP = std::cos(phase), sinP = std::sin(phase);
  Complex s0_s(s0_c.real() * cosP + s0_c.imag() * sinP, s0_c.imag() * cosP - s0_c.real() * sinP);
  Complex s1_s(s1_c.real() * cosP - s1_c.imag() * sinP, s1_c.imag() * cosP + s1_c.real() * sinP);

  // 3. Channel Alignment (Match S0/S1 to S2 reference)
  Complex wA0(wA0_r, wA0_i), wA1(wA1_r, wA1_i);
  Complex out_fund = wA0 * s0_s + wA1 * s1_s;
  Complex err_vec = s2_c - out_fund;

  // Final combined signal (1-2-1 Triple-QSD Matrix)
  // Synthesizes 0.25*S0 + 0.50*S2 + 0.25*S1 when weights are 0.5
  Complex err = 0.5f * out_fund + 0.5f * s2_c;

  // 4. Update LMS weights (only if calibration is active)
  if (calibrationActive.load()) {
    float mu = lmsMu.load();
    
    // Update I/Q correctors (minimize E[S_corr^2])
    constexpr float eps = 1e-6f;
    Complex u0 = s0_c * s0_c; accIQ0_r += u0.real(); accIQ0_i += u0.imag(); pIQ0 += std::norm(s0);
    Complex u1 = s1_c * s1_c; accIQ1_r += u1.real(); accIQ1_i += u1.imag(); pIQ1 += std::norm(s1);
    Complex u2 = s2_c * s2_c; accIQ2_r += u2.real(); accIQ2_i += u2.imag(); pIQ2 += std::norm(s2);
    
    // Update Alignment correctors - simple gradient descent
    Complex ua0 = err_vec * std::conj(s0_s); accA0_r += ua0.real(); accA0_i += ua0.imag();
    Complex ua1 = err_vec * std::conj(s1_s); accA1_r += ua1.real(); accA1_i += ua1.imag();

    if (++sampleBlockCounter >= 4093 * 4) { // Longer integration for calibration stability
      auto updateW = [&](float& wr, float& wi, float ar, float ai, float p) {
          if (p + eps > 1e-10f) { wr += mu * ar / (p + eps); wi += mu * ai / (p + eps); }
      };
      updateW(wIQ0_r, wIQ0_i, accIQ0_r, accIQ0_i, pIQ0);
      updateW(wIQ1_r, wIQ1_i, accIQ1_r, accIQ1_i, pIQ1);
      updateW(wIQ2_r, wIQ2_i, accIQ2_r, accIQ2_i, pIQ2);
      
      float muAlign = mu * 2.0f;
      wA0_r += muAlign * accA0_r / (4093.0f * 4.0f); wA0_i += muAlign * accA0_i / (4093.0f * 4.0f);
      wA1_r += muAlign * accA1_r / (4093.0f * 4.0f); wA1_i += muAlign * accA1_i / (4093.0f * 4.0f);
      
      // Clamp and Finalize
      auto clampW = [](float& wr, float& wi) {
          float mag = std::sqrt(wr*wr + wi*wi);
          if (mag > 2.0f) { wr *= 2.0f/mag; wi *= 2.0f/mag; }
      };
      clampW(wIQ0_r, wIQ0_i); clampW(wIQ1_r, wIQ1_i); clampW(wIQ2_r, wIQ2_i);
      clampW(wA0_r, wA0_i); clampW(wA1_r, wA1_i);

      // Diagnostics / Results
      auto toGain = [](float r) { return 20.0f * std::log10(std::max(0.1f, 1.0f - 2.0f * r)); };
      auto toPhase = [](float i) { return -2.0f * i * 180.0f / M_PI; };
      
      float g0 = toGain(wIQ0_r), p0 = toPhase(wIQ0_i);
      float g1 = toGain(wIQ1_r), p1 = toPhase(wIQ1_i);
      float g2 = toGain(wIQ2_r), p2 = toPhase(wIQ2_i);
      float a0 = std::arg(Complex(wA0_r, wA0_i)) * 180.0f / M_PI;
      float a1 = std::arg(Complex(wA1_r, wA1_i)) * 180.0f / M_PI;

      std::cout << "\n[DSP] Calibration Complete. Append to config/calibration.lua:" << std::endl;
      printf("calibration.addResults({\n  { gain=%+.3f, phase=%+.2f }, -- QSD0\n  { gain=%+.3f, phase=%+.2f }, -- QSD1\n  { gain=%+.3f, phase=%+.2f }  -- QSD2\n})\n",
             g0, p0, g1, p1, g2, p2);
      std::cout << "[DSP] (Note: ALIGN phases were " << a0 << ", " << a1 << " deg)" << std::endl;

      calibrationActive.store(false);
      sampleBlockCounter = 0;
      
      // Update persistent diagnostics
      dspDiag.gainErr0.store(g0); dspDiag.phaseErr0.store(p0);
      dspDiag.gainErr1.store(g1); dspDiag.phaseErr1.store(p1);
      dspDiag.gainErr2.store(g2); dspDiag.phaseErr2.store(p2);
      dspDiag.alignPhase0.store(a0); dspDiag.alignPhase1.store(a1);
    }
  }

  // Output after all suppression
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
