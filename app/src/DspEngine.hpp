/**
 * @file DspEngine.hpp
 * @brief IQ Signal Processing and Image Rejection
 */

#pragma once

#include <vector>
#include <atomic>
#include <mutex>
#include "transport/IQFrame.hpp"
#include "Demodulator.hpp"
#include "BasebandFilter.hpp"
#include "RateAdaptiveBuffer.hpp"

using namespace nexrx;

struct DspDiagnostics {
  std::atomic<uint64_t> framesProcessed{0};
  std::atomic<float> signalRms{0.0f};
  std::atomic<float> maxRaw{0.0f};
  std::atomic<float> maxAudio{0.0f};
  std::atomic<float> lmsWeightR{0.0f};
  std::atomic<float> lmsWeightI{0.0f};
};

class DspEngine {
public:
  static constexpr int FFT_SIZE = 1024;

  DspEngine();

  void processIQFrame(const nexrx::IQFrame& frame);
  void computeSpectrum();
  
  // Property controls
  void setVfo(double freqHz);
  void setQsdOffset(double offsetKhz);
  void setRfGain(float db) { rfGainDB.store(db); }
  void setLmsMu(float mu) { lmsMu = mu; }
  
  // Data access
  std::vector<float> getSpectrumData();
  const DspDiagnostics& getDiagnostics() const { return dspDiag; }
  RateAdaptiveBuffer<float>& getAudioBuffer() { return audioBuffer; }
  Demodulator& getDemod() { return demod; }
  BasebandFilter& getFilter() { return basebandFilter; }

private:
  void fftInPlace(float* re, float* im, size_t n);

  // State
  std::mutex spectrumMutex;
  std::vector<float> spectrumData;
  std::vector<float> iqBuffer;
  std::atomic<size_t> iqBufferWritePos{0};

  std::atomic<float> rfGainDB{20.0f};
  double qsdOffsetKhz = 12.0;
  double lastVFOHz = 14.2e6;
  float lmsMu = 0.001f;
  float lmsW0_r = 0.0f;
  float lmsW0_i = 0.0f;
  float lmsAcc_r = 0.0f;
  float lmsAcc_i = 0.0f;
  uint32_t sampleBlockCounter = 0;

  float shiftCos0 = 1.0f, shiftSin0 = 0.0f;
  float shiftCos1 = 1.0f, shiftSin1 = 0.0f;
  float shiftCosD = 1.0f, shiftSinD = 0.0f;
  float lastShiftK = -1.0f;

  float dc0_i = 0.0f, dc0_q = 0.0f;
  float dc1_i = 0.0f, dc1_q = 0.0f;

  bool audioDecimateSkip = false;

  Demodulator demod;
  BasebandFilter basebandFilter{96000};
  DspDiagnostics dspDiag;
  RateAdaptiveBuffer<float> audioBuffer;
};
