#pragma once

#include <functional>
#include <string>
#include <vector>
#include <cstdint>
#include "transport/IQFrame.hpp"

namespace nexrx {

class RadioSource {
public:
  using FrameCallback = std::function<void(const IQFrame&)>;
  using BatchCallback = std::function<void(const std::vector<IQFrame>&)>;

  virtual ~RadioSource() = default;

  virtual void shutdown() = 0;

  virtual void setFrameCallback(FrameCallback callback) = 0;
  virtual void setBatchCallback(BatchCallback callback) = 0;

  virtual bool startReceiving() = 0;
  virtual void stopReceiving() = 0;
  virtual bool isReceiving() const = 0;

  virtual bool startStream() = 0;
  virtual bool stopStream() = 0;

  // Base capabilities
  virtual bool setVFO(double freqHz, double offsetHz) = 0;

  // Expanded API (stubs for generic SDRs, overridden by specific hardware like NexRx)
  virtual bool setAtten(int dbValue) { return false; }
  virtual bool setPGAGain(int code) { return false; }
  virtual bool setAGCMode(int mode) { return false; }
  virtual bool setISGFreq(double freqHz) { return false; }
  virtual bool setISGEnable(bool enabled) { return false; }
  virtual bool setHpfBypass(bool bypass) { return false; }
  virtual bool setBpfIndex(int index) { return false; }
  virtual bool setTrMode(int mode) { return false; }
  virtual bool setQsdOffsetKHz(double khz) { return false; }
  virtual uint64_t getTimestamp() { return 0; }
  virtual std::vector<uint8_t> getState() { return {}; }
  virtual void pollStateAsync() {}
  virtual bool sendCalibrationStimulus(double freqHz, uint64_t durationMs) { return false; }
};

} // namespace nexrx