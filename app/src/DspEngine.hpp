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
  std::atomic<float> lmsWeightR{0.0f}; // Magnitude of wA0
  std::atomic<float> lmsWeightI{0.0f}; // Magnitude of wA1
  std::atomic<float> gainErr0{0.0f};
  std::atomic<float> phaseErr0{0.0f};
  std::atomic<float> gainErr1{0.0f};
  std::atomic<float> phaseErr1{0.0f};
  std::atomic<float> gainErr2{0.0f};
  std::atomic<float> phaseErr2{0.0f};
  std::atomic<float> alignPhase0{0.0f};
  std::atomic<float> alignPhase1{0.0f};
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
  double getQsdOffset() const { return qsdOffsetKhz; }
  void setRfGain(float db) { rfGainDB.store(db); }
  void setLmsMu(float mu) { lmsMu = mu; }
  void setLmsEnabled(bool en) { lmsEnabled.store(en); }
  void setMatrixBypass(bool en) { matrixBypass.store(en); }
  
  // Calibration
  void setCalibration(int ch, float gainDB, float phaseDeg, float alignR = 0.5f, float alignI = 0.0f);
  void startManualCalibration();
  bool isCalibrating() const { return calibrationActive.load(); }

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
  double lastK_hz = -1.0;
  double shiftCos = 1.0, shiftSin = 0.0;
  double shiftCos_d = 1.0, shiftSin_d = 0.0;
  double lastVFOHz = 14.2e6;
  std::atomic<float> lmsMu{0.01f};
  std::atomic<bool> lmsEnabled{true};
  std::atomic<bool> matrixBypass{false};
  std::atomic<bool> calibrationActive{false};
  
  struct QsdCal {
    float gainErrDB = 0.0f;
    float phaseErrDeg = 0.0f;
    float alignR = 0.5f;
    float alignI = 0.0f;
  };
  QsdCal staticCal[3];
  
  // Independent I/Q correction weights for each QSD
  float wIQ0_r = 0, wIQ0_i = 0;
  float wIQ1_r = 0, wIQ1_i = 0;
  float wIQ2_r = 0, wIQ2_i = 0;
  
  // Alignment weights (to match QSD0/1 to QSD2 reference)
  float wA0_r = 0.5f, wA0_i = 0;
  float wA1_r = 0.5f, wA1_i = 0;

  // Accumulators
  float accIQ0_r = 0, accIQ0_i = 0, pIQ0 = 0;
  float accIQ1_r = 0, accIQ1_i = 0, pIQ1 = 0;
  float accIQ2_r = 0, accIQ2_i = 0, pIQ2 = 0;
  float accA0_r = 0, accA0_i = 0, pA0 = 0;
  float accA1_r = 0, accA1_i = 0, pA1 = 0;

  uint64_t totalSamplesProcessed = 0;
  uint32_t sampleBlockCounter = 0;

  float dc0_i = 0.0f, dc0_q = 0.0f;
  float dc1_i = 0.0f, dc1_q = 0.0f;
  float dc2_i = 0.0f, dc2_q = 0.0f;

  bool audioDecimateSkip = false;

  Demodulator demod;
  BasebandFilter basebandFilter{96000};
  DspDiagnostics dspDiag;
  RateAdaptiveBuffer<float> audioBuffer;
};
