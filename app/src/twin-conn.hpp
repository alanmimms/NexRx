#pragma once

#include "tcp-control-client.hpp"
#include "udp-stream-client.hpp"
#include "transport/IQFrame.hpp"

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

class TwinConn {
public:
    using FrameCallback = std::function<void(const IQFrame&)>;
    using BatchCallback = std::function<void(const std::vector<IQFrame>&)>;

    TwinConn() = default;
    ~TwinConn();

    TwinConn(const TwinConn&) = delete;
    TwinConn& operator=(const TwinConn&) = delete;

    bool initialize(const TwinConfig& cfg = TwinConfig{});
    void shutdown();

    [[nodiscard]] bool isConnected() const { return connected; }

    void setFrameCallback(FrameCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        frameCallback = std::move(callback);
    }

    void setBatchCallback(BatchCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        batchCallback = std::move(callback);
    }

    bool startReceiving();
    void stopReceiving();

    [[nodiscard]] bool isReceiving() const { return receiving; }
    size_t pollFrames(size_t maxFrames = 100);

    /* Control Commands */
    bool setVFO(double freqHz, double offsetHz);
    bool setAtten(int dbValue);
    bool setPGAGain(int code);
    bool setAGCMode(int mode);
    bool setIsgFreq(double freqHz);
    bool startStream();
    bool stopStream();
    uint64_t getTimestamp();

    std::vector<uint8_t> sendCBORRequest(const std::string& cmdId, const std::vector<uint8_t>& argsCBOR);

    [[nodiscard]] uint64_t framesReceived() const { return framesReceivedCount; }
    [[nodiscard]] uint64_t lastSequence() const { return lastSequenceReceived; }

private:
    void receiveLoop();
    std::vector<uint8_t> processResponse(const std::vector<uint8_t>& responseData, const std::string& cmdId);

    TwinConfig config;
    std::unique_ptr<TCPControlClient> control;
    std::unique_ptr<UDPStreamClient> stream;
    bool connected = false;

    mutable std::mutex callbackMutex;
    FrameCallback frameCallback;
    BatchCallback batchCallback;

    std::thread receiveThread;
    std::atomic<bool> receiving{false};
    std::atomic<bool> stopRequested{false};

    std::atomic<uint64_t> framesReceivedCount{0};
    std::atomic<uint64_t> lastSequenceReceived{0};

    IQFrame lastFrameReceived{};
    std::vector<IQFrame> frameBuffer;
};

} // namespace nexrx
