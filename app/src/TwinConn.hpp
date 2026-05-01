#pragma once

#include "RadioSource.hpp"
#include "TCPControlClient.hpp"
#include "UDPStreamClient.hpp"
#include "transport/IQFrame.hpp"
#include "Control.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <mutex>

namespace nexrx {

struct TwinConfig {
  std::string host = "127.0.0.1";
  uint16_t controlPort = 5000;
  uint16_t streamPort = 5001;
  size_t frameBufferSize = 1024;
  size_t receiveBufferSize = 8192;
  bool verbose = false;
};

class TwinConn : public RadioSource {
public:
  TwinConn() = default;
  ~TwinConn() override;

  TwinConn(const TwinConn&) = delete;
  TwinConn& operator=(const TwinConn&) = delete;

  bool initialize(const TwinConfig& cfg = TwinConfig{});
  void shutdown() override;

  [[nodiscard]] bool isConnected() const { return connected; }

  void setFrameCallback(FrameCallback callback) override {
    std::lock_guard<std::mutex> lock(callbackMutex);
    frameCallback = std::move(callback);
  }

  void setBatchCallback(BatchCallback callback) override {
    std::lock_guard<std::mutex> lock(callbackMutex);
    batchCallback = std::move(callback);
  }

  bool startReceiving() override;
  void stopReceiving() override;

  [[nodiscard]] bool isReceiving() const override { return receiving; }
  size_t pollFrames(size_t maxFrames = 100);

  /* Control Commands */
  bool setVFO(double freqHz, double offsetHz) override;
  bool setAtten(int dbValue) override;
  bool setPGAGain(int code) override;
  bool setAGCMode(int mode) override;
  bool setISGFreq(double freqHz) override;
  bool setISGEnable(bool enabled) override;
  bool setHpfBypass(bool bypass) override;
  bool setBpfIndex(int index) override;
  bool setTrMode(int mode) override;
  bool setQsdOffsetKHz(double khz) override;
  bool startStream() override;
  bool stopStream() override;
  uint64_t getTimestamp() override;
  std::vector<uint8_t> getState() override;
  void pollStateAsync() override; // Called by background thread
  bool sendCalibrationStimulus(double freqHz, uint64_t durationMs) override;

  std::vector<uint8_t> sendCBORRequest(uint32_t cmdId, const std::vector<uint8_t>& argsCBOR);

  [[nodiscard]] uint64_t getFramesReceived() const { return framesReceivedCount; }
  [[nodiscard]] uint64_t getLastSequence() const { return lastSequenceReceived; }

private:
  void receiveLoop();
  std::vector<uint8_t> processResponse(const std::vector<uint8_t>& responseData, uint32_t cmdId);

  TwinConfig config;
  std::unique_ptr<TCPControlClient> control;
  std::mutex controlMutex; // Protect TCP control connection

  std::unique_ptr<UDPStreamClient> stream;
  bool connected = false;

  mutable std::mutex callbackMutex;
  FrameCallback frameCallback;
  BatchCallback batchCallback;

  std::vector<uint8_t> cachedStateCBOR;
  std::mutex stateMutex; // Protect state cache

  std::thread receiveThread;
  std::atomic<bool> receiving{false};
  std::atomic<bool> stopRequested{false};

  std::atomic<uint64_t> framesReceivedCount{0};
  std::atomic<uint64_t> lastSequenceReceived{0};

  IQFrame lastFrameReceived{};
  std::vector<IQFrame> frameBuffer;
};

} // namespace nexrx
