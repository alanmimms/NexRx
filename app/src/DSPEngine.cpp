/**
 * @file DSPEngine.cpp
 * @brief Implementation of IQ Signal Processing
 */

#include "DSPEngine.hpp"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

DSPEngine::DSPEngine() {
  iqBuffer.assign(FFT_SIZE * 2, 0.0f);
  spectrumData.assign(FFT_SIZE, -100.0f);
  audioBuffer.configure(nexrx::BufferConfig{32768});
  demod.setSampleRate(384000.0f);
  
  // Default alignment for Dual-OSD
  staticCal[0].alignR = 1.0f; staticCal[0].alignI = 0.0f;
  staticCal[1].alignR = 1.0f; staticCal[1].alignI = 0.0f;
}

void DSPEngine::setVfo(double freqHz) {
  lastVFOHz = freqHz;
  basebandFilter.recompute();
}

void DSPEngine::setModeId(int id) {
  Demodulator::Mode mode = static_cast<Demodulator::Mode>(id);
  demod.setMode(mode);
  
  // Configure baseband filter for sideband selection
  if (mode == Demodulator::Mode::LSB) {
    basebandFilter.setBandpassCenter(-1500.0f);
    basebandFilter.setBandpassWidth(2400.0f);
  } else if (mode == Demodulator::Mode::USB) {
    basebandFilter.setBandpassCenter(1500.0f);
    basebandFilter.setBandpassWidth(2400.0f);
  } else if (mode == Demodulator::Mode::CW) {
    basebandFilter.setBandpassCenter(700.0f);
    basebandFilter.setBandpassWidth(500.0f);
  } else if (mode == Demodulator::Mode::AM) {
    basebandFilter.setBandpassCenter(0.0f);
    basebandFilter.setBandpassWidth(10000.0f);
  }
  basebandFilter.recompute();
}

void DSPEngine::setCalibration(int ch, float gainDB, float phaseDeg, float alignR, float alignI) {
  if (ch < 0 || ch > 1) return;
  staticCal[ch] = {gainDB, phaseDeg, alignR, alignI};
  
  // Apply I/Q correction weights
  float g = std::pow(10.0f, -gainDB / 20.0f);
  float p = -phaseDeg * M_PI / 180.0f;
  float wr = (1.0f - g)/2.0f;
  float wi = p/2.0f;
  
  if (ch == 0) { wIQ0_r = wr; wIQ0_i = wi; wA0_r = alignR; wA0_i = alignI; }
  else if (ch == 1) { wIQ1_r = wr; wIQ1_i = wi; }
}

void DSPEngine::startManualCalibration() {
  std::cout << "[DSP] Starting high-precision calibration sequence..." << std::endl;
  // Reset weights to "raw" state for discovery
  wIQ0_r = wIQ0_i = wIQ1_r = wIQ1_i = 0;
  wA0_r = 1.0f; wA0_i = 0;
  
  // Clear accumulators
  accIQ0_r = accIQ0_i = pIQ0 = 0;
  accIQ1_r = accIQ1_i = pIQ1 = 0;
  accA0_r = accA0_i = pA0 = 0;
  
  sampleBlockCounter = 0;
  calibrationActive.store(true);
}

void DSPEngine::setOSDOffset(double offsetKhz) {
  osdOffsetKhz = offsetKhz;
  basebandFilter.recompute();
}

std::vector<float> DSPEngine::getSpectrumData() {
  std::lock_guard<std::mutex> l(spectrumMutex);
  return spectrumData;
}

#include <complex>

using Complex = std::complex<float>;

void DSPEngine::processIQFrame(const nexrx::IQFrame& frame) {
  float i0, q0, i1, q1;
  frame.osd[0].toFloat(i0, q0); 
  frame.osd[1].toFloat(i1, q1); 
  
  // DC Offset Correction
  constexpr float dcAlphaFast = 0.00025f; // ~15Hz cutoff at 384ksps
  
  dc0_i = (1.0f - dcAlphaFast) * dc0_i + dcAlphaFast * i0;
  dc0_q = (1.0f - dcAlphaFast) * dc0_q + dcAlphaFast * q0;
  dc1_i = (1.0f - dcAlphaFast) * dc1_i + dcAlphaFast * i1;
  dc1_q = (1.0f - dcAlphaFast) * dc1_q + dcAlphaFast * q1;

  i0 -= dc0_i; q0 -= dc0_q;
  i1 -= dc1_i; q1 -= dc1_q;

  // 1. Independent Blind I/Q Correction for both mixers
  // S_corr = S - w*conj(S)
  Complex s0(i0, q0), s1(i1, q1);
  Complex wIQ0(wIQ0_r, wIQ0_i), wIQ1(wIQ1_r, wIQ1_i);
  
  Complex s0_c = s0 - wIQ0 * std::conj(s0);
  Complex s1_c = s1 - wIQ1 * std::conj(s1);

  // 2. High-Precision Frequency Alignment
  // Use incremental rotation (phasors) to avoid trig calls per sample
  constexpr double sampleRate = 384000.0;
  double k_hz = osdOffsetKhz * 1000.0;
  
  if (std::abs(k_hz - lastK_hz) > 0.1) {
    double phaseInc = 2.0 * M_PI * k_hz / sampleRate;
    shiftCos_d = std::cos(phaseInc);
    shiftSin_d = std::sin(phaseInc);
    lastK_hz = k_hz;
  }

  // Shift OSD0 DOWN by k: (i + j*q) * (cos - j*sin)
  Complex s0_s(s0_c.real() * shiftCos + s0_c.imag() * shiftSin, s0_c.imag() * shiftCos - s0_c.real() * shiftSin);
  // Shift OSD1 UP by k: (i + j*q) * (cos + j*sin)
  Complex s1_s(s1_c.real() * shiftCos - s1_c.imag() * shiftSin, s1_c.imag() * shiftCos + s1_c.real() * shiftSin);

  // 3. Dual-OSD Coherent Summation
  Complex wA0(wA0_r, wA0_i);
  Complex combined = (wA0 * s0_s + s1_s) * 0.5f;
  
  // Final combined signal
  Complex err = matrixBypass.load() ? s0_s : combined;

  // Update diagnostics for UI
  dspDiag.lmsWeightR.store(std::abs(Complex(wA0_r, wA0_i)));
  dspDiag.lmsWeightI.store(0.0f);

  // 4. Update Calibration (only if active)
  if (calibrationActive.load()) {
    // Accumulate for I/Q Correctors (S_corr = S - w S*)
    auto accIQ = [](Complex s, float& ar, float& ai, float& p) {
        Complex u = s * s; ar += u.real(); ai += u.imag(); p += std::norm(s);
    };
    accIQ(s0, accIQ0_r, accIQ0_i, pIQ0);
    accIQ(s1, accIQ1_r, accIQ1_i, pIQ1);
    
    // Accumulate for Alignment (Match shifted OSD0 to OSD1 reference)
    auto accA = [](Complex ref, Complex s_s, float& ar, float& ai, float& p) {
        Complex u = ref * std::conj(s_s); ar += u.real(); ai += u.imag(); p += std::norm(s_s);
    };
    Complex s0_raw_s(s0.real() * shiftCos + s0.imag() * shiftSin, s0.imag() * shiftCos - s0.real() * shiftSin);
    accA(s1, s0_raw_s, accA0_r, accA0_i, pA0);

    if (++sampleBlockCounter >= 4093 * 8) { // 32,744 samples (~340ms)
      auto solveW = [](float ar, float ai, float p, float& wr, float& wi) {
          if (p > 1e-10f) { wr = 0.5f * ar / p; wi = 0.5f * ai / p; }
      };
      solveW(accIQ0_r, accIQ0_i, pIQ0, wIQ0_r, wIQ0_i);
      solveW(accIQ1_r, accIQ1_i, pIQ1, wIQ1_r, wIQ1_i);
      
      auto solveWA = [](float ar, float ai, float p, float& wr, float& wi) {
          if (p > 1e-10f) { wr = ar / p; wi = ai / p; }
      };
      solveWA(accA0_r, accA0_i, pA0, wA0_r, wA0_i);

      // Final Diagnostics / Results
      auto toGain = [](float r) { return 20.0f * std::log10(std::max(0.1f, 1.0f - 2.0f * r)); };
      auto toPhase = [](float i) { return -2.0f * i * 180.0f / M_PI; };
      
      float g0 = toGain(wIQ0_r), p0 = toPhase(wIQ0_i);
      float g1 = toGain(wIQ1_r), p1 = toPhase(wIQ1_i);

      std::cout << "\n[DSP] Calibration Complete. Append to config/calibration.lua:" << std::endl;
      printf("calibration.addResults({\n"
             "  { gain=%+.3f, phase=%+.2f, align_r=%.4f, align_i=%.4f }, -- OSD0\n"
             "  { gain=%+.3f, phase=%+.2f, align_r=1.0000, align_i=0.0000 }  -- OSD1\n"
             "})\n",
             g0, p0, wA0_r, wA0_i, 
             g1, p1);

      calibrationActive.store(false);
      sampleBlockCounter = 0;
      
      dspDiag.gainErr0.store(g0); dspDiag.phaseErr0.store(p0);
      dspDiag.gainErr1.store(g1); dspDiag.phaseErr1.store(p1);
      dspDiag.alignPhase0.store(std::arg(Complex(wA0_r, wA0_i)) * 180.0f / M_PI);
    }
  }

  // 1. Write to spectrum buffer BEFORE gain/filtering (Raw combined signal)
  size_t pos = iqBufferWritePos.load(std::memory_order_relaxed); 
  iqBuffer[pos*2] = err.real(); iqBuffer[pos*2+1] = err.imag();
  iqBufferWritePos.store((pos+1)%FFT_SIZE, std::memory_order_release);

  // 2. Apply digital gain and RF metering
  float iF = err.real();
  float qF = err.imag();
  
  float rfGain = std::pow(10.0f, rfGainDB.load() / 20.0f);
  iF *= rfGain; qF *= rfGain;
  float maxR = std::max(std::abs(iF), std::abs(qF));
  if (maxR > dspDiag.maxRaw.load()) dspDiag.maxRaw.store(maxR);

  // 4. Digital Tuning Shift (to bring selected signalbox to DC for demod)
  if (std::abs(tuningOffsetHz - lastTune_hz) > 0.1) {
    constexpr double sampleRate = 384000.0;
    double phaseInc = -2.0 * M_PI * tuningOffsetHz / sampleRate;
    tuneCos_d = std::cos(phaseInc);
    tuneSin_d = std::sin(phaseInc);
    lastTune_hz = tuningOffsetHz;
  }
  float iT = iF * (float)tuneCos - qF * (float)tuneSin;
  float qT = qF * (float)tuneCos + iF * (float)tuneSin;
  iF = iT; qF = qT;

  basebandFilter.process(iF, qF);

  // Advance phasors for next sample
  double nextCos = shiftCos * shiftCos_d - shiftSin * shiftSin_d;
  double nextSin = shiftSin * shiftCos_d + shiftCos * shiftSin_d;
  shiftCos = nextCos; shiftSin = nextSin;
  
  double nextTuneCos = tuneCos * tuneCos_d - tuneSin * tuneSin_d;
  double nextTuneSin = tuneSin * tuneCos_d + tuneCos * tuneSin_d;
  tuneCos = nextTuneCos; tuneSin = nextTuneSin;

  if ((totalSamplesProcessed & 0x3FFF) == 0) {
    double mag = std::sqrt(shiftCos * shiftCos + shiftSin * shiftSin);
    shiftCos /= mag; shiftSin /= mag;
    double tMag = std::sqrt(tuneCos * tuneCos + tuneSin * tuneSin);
    tuneCos /= tMag; tuneSin /= tMag;
  }
  
  float aOut = demod.process(iF, qF);
  dspDiag.signalRms.store(demod.getSignalLevelRMS());
  if (std::abs(aOut) > dspDiag.maxAudio.load()) dspDiag.maxAudio.store(std::abs(aOut));
  
  if (!audioDecimateSkip) audioBuffer.write(aOut);
  audioDecimateSkip = !audioDecimateSkip;
  totalSamplesProcessed++;
  dspDiag.framesProcessed++;
}

void DSPEngine::fftInPlace(float* re, float* im, size_t n) {
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

void DSPEngine::computeSpectrum() {
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
  else { for (size_t k=0; k<FFT_SIZE; ++k) avgS[k] = 0.6f*lS[k] + 0.4f*avgS[k]; }
  { std::lock_guard<std::mutex> l(spectrumMutex); spectrumData = avgS; }
}
