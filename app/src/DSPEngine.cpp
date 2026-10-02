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
  demod.setSampleRate(48000.0f);
  
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

// 25-tap anti-aliasing FIR filter coefficients for 8:1 decimation (384ksps -> 48ksps, fc = 18 kHz)
static constexpr float decimCoeffs[25] = {
  -0.00090794f, -0.00030340f,  0.00098332f,  0.00400265f,  0.00975136f,
   0.01887400f,  0.03141413f,  0.04668369f,  0.06329286f,  0.07934581f,
   0.09276679f,  0.10168846f,  0.10481654f,  0.10168846f,  0.09276679f,
   0.07934581f,  0.06329286f,  0.04668369f,  0.03141413f,  0.01887400f,
   0.00975136f,  0.00400265f,  0.00098332f, -0.00030340f, -0.00090794f
};

void DSPEngine::processIQFrame(const nexrx::IQFrame& frame) {
  float iF, qF;
  frame.toFloat(iF, qF);
  
  // DC Offset Correction
  constexpr float dcAlphaFast = 0.00025f; // ~15Hz cutoff at 384ksps
  dc0_i = (1.0f - dcAlphaFast) * dc0_i + dcAlphaFast * iF;
  dc0_q = (1.0f - dcAlphaFast) * dc0_q + dcAlphaFast * qF;
  iF -= dc0_i;
  qF -= dc0_q;

  // 1. Write to spectrum buffer BEFORE tuning/gain (Raw 384ksps input around VFO)
  size_t pos = iqBufferWritePos.load(std::memory_order_relaxed); 
  iqBuffer[pos * 2] = iF;
  iqBuffer[pos * 2 + 1] = qF;
  iqBufferWritePos.store((pos + 1) % FFT_SIZE, std::memory_order_release);

  // 2. Apply digital gain and RF metering
  float rfGain = std::pow(10.0f, rfGainDB.load() / 20.0f);
  iF *= rfGain;
  qF *= rfGain;
  float maxR = std::max(std::abs(iF), std::abs(qF));
  if (maxR > dspDiag.maxRaw.load()) {
    dspDiag.maxRaw.store(maxR);
  }

  // 3. Digital Tuning Shift (to bring selected frequency to DC for demod)
  if (std::abs(tuningOffsetHz - lastTune_hz) > 0.1) {
    constexpr double sampleRate = 384000.0;
    double phaseInc = -2.0 * M_PI * tuningOffsetHz / sampleRate;
    tuneCos_d = std::cos(phaseInc);
    tuneSin_d = std::sin(phaseInc);
    lastTune_hz = tuningOffsetHz;
  }
  float iT = iF * (float)tuneCos - qF * (float)tuneSin;
  float qT = qF * (float)tuneCos + iF * (float)tuneSin;
  iF = iT;
  qF = qT;

  // Advance tuning phasors
  double nextTuneCos = tuneCos * tuneCos_d - tuneSin * tuneSin_d;
  double nextTuneSin = tuneSin * tuneCos_d + tuneCos * tuneSin_d;
  tuneCos = nextTuneCos;
  tuneSin = nextTuneSin;

  if ((totalSamplesProcessed & 0x3FFF) == 0) {
    double tMag = std::sqrt(tuneCos * tuneCos + tuneSin * tuneSin);
    tuneCos /= tMag;
    tuneSin /= tMag;
  }

  // Push tuned 384ksps sample into decimation delay line
  decimHistoryI[decimRingPos] = iF;
  decimHistoryQ[decimRingPos] = qF;
  decimRingPos = (decimRingPos + 1) & decimBufMask;

  // 8:1 decimation from 384ksps to 48ksps
  if (++decimPhase >= audioDecimationFactor) {
    decimPhase = 0;

    // 25-tap anti-aliasing FIR filter (fc = 18 kHz)
    float i48 = 0.0f;
    float q48 = 0.0f;
    for (int tap = 0; tap < decimTaps; ++tap) {
      int idx = (decimRingPos - 1 - tap) & decimBufMask;
      float c = decimCoeffs[tap];
      i48 += decimHistoryI[idx] * c;
      q48 += decimHistoryQ[idx] * c;
    }

    basebandFilter.process(i48, q48);
    float aOut = demod.process(i48, q48);

    dspDiag.signalRms.store(demod.getSignalLevelRMS());
    if (std::abs(aOut) > dspDiag.maxAudio.load()) dspDiag.maxAudio.store(std::abs(aOut));

    audioBuffer.write(aOut);
  }
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
