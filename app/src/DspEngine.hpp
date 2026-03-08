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
  std::atomic<float> lmsWeightR{0.0f}; // Magnitude of w0
  std::atomic<float> lmsWeightI{0.0f}; // Magnitude of w1
  std::atomic<float> gainErr0{0.0f};
  std::atomic<float> phaseErr0{0.0f};
  std::atomic<float> gainErr1{0.0f};
  std::atomic<float> phaseErr1{0.0f};
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
  std::atomic<float> lmsMu{0.01f};
  
  // LMS weights and power accumulators for QSD0 and QSD1 I/Q correction
  float w0_r = 0.0f, w0_i = 0.0f;
  float w1_r = 0.0f, w1_i = 0.0f;
  float acc0_r = 0.0f, acc0_i = 0.0f;
  float acc1_r = 0.0f, acc1_i = 0.0f;
  float pwr0 = 0.0f, pwr1 = 0.0f;
  uint64_t totalSamplesProcessed = 0;
  uint32_t sampleBlockCounter = 0;

  float dc0_i = 0.0f, dc0_q = 0.0f;
  float dc1_i = 0.0f, dc1_q = 0.0f;

  bool audioDecimateSkip = false;

  Demodulator demod;
  BasebandFilter basebandFilter{96000};
  DspDiagnostics dspDiag;
  RateAdaptiveBuffer<float> audioBuffer;
};
