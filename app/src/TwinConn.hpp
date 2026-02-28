// NexRx App - Twin Connection (Cross-platform)
//
// Connects to the digital twin simulation via TCP (control)
// and UDP (I/Q streaming).
//
// Copyright 2026 NexRx Project - MIT License

#pragma once

#include "TcpControlClient.hpp"
#include "UdpStreamClient.hpp"
#include "transport/IQFrame.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace nexrx {

//======================================================================
// Twin Connection Configuration
//======================================================================
struct TwinConfig {
    std::string host = "127.0.0.1";       // Twin address
    uint16_t controlPort = 5000;           // TCP control port
    uint16_t streamPort = 5001;            // UDP stream port
    size_t frameBufferSize = 1024;         // Internal frame buffer
    size_t receiveBufferSize = 8192;       // UDP receive buffer
    bool verbose = false;
};

//======================================================================
// Twin Connection
//
// Connects to the digital twin's network transport and
// receives I/Q frames for processing.
//======================================================================
class TwinConn {
public:
    using FrameCallback = std::function<void(const IQFrame&)>;
    using BatchCallback = std::function<void(const std::vector<IQFrame>&)>;

    TwinConn() = default;
    ~TwinConn();

    // Non-copyable
    TwinConn(const TwinConn&) = delete;
    TwinConn& operator=(const TwinConn&) = delete;

    // Initialize and connect to twin
    bool initialize(const TwinConfig& config = TwinConfig{});

    // Shutdown and disconnect
    void shutdown();

    // Check if connected
    [[nodiscard]] bool isConnected() const { return connected_; }

    // Set callback for each received frame
    void setFrameCallback(FrameCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        frameCallback_ = std::move(callback);
    }

    // Set callback for batch of frames (more efficient)
    void setBatchCallback(BatchCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        batchCallback_ = std::move(callback);
    }

    // Start receiving frames in background thread
    bool startReceiving();

    // Stop receiving
    void stopReceiving();

    // Check if receiving
    [[nodiscard]] bool isReceiving() const { return receiving_; }

    // Poll for frames (non-blocking, returns number received)
    size_t pollFrames(size_t maxFrames = 100);

    //------------------------------------------------------------------
    // Control Commands (via TCP/CBOR)
    //------------------------------------------------------------------

    // Set VFO for a specific QSD channel (0, 1, or 2)
    bool setQsdVfo(int index, double freq_hz);

    // Set attenuator pad enabled state
    bool setAtten(int db_value, bool enabled);

    // Set preselector capacitor (0-10)
    bool setPreselectorCap(int index, bool enabled);

    // Set preselector inductor (index 0 for L701 bypass)
    bool setPreselectorInd(int index, bool enabled);

    // PGA (MAX9939) Control
    bool setPgaGain(int index, double gain_db);

    // ISG (Internal Signal Generator) Control
    bool setIsgEnable(bool enabled);
    bool setIsgFreq(double freq_hz);

    // Audio Codec (AK5578) Control
    bool setCodecConfig(int rate, const std::vector<int>& channel_map, double gain, int filter_type);

    // Calibration storage (JSON strings)
    bool setCalibration(const std::string& type, const std::string& json_data);
    std::string getCalibration(const std::string& type);

    // Get twin status (returns JSON string)
    std::string getStatus();

    // Get hardware configuration (returns JSON string)
    std::string getHardwareConfig();

    // Start/stop streaming
    bool startStream();
    bool stopStream();

    // Send raw CBOR request (returns CBOR response payload)
    std::vector<uint8_t> sendCborRequest(const std::string& cmd_id, const std::vector<uint8_t>& args_cbor);

    //------------------------------------------------------------------
    // Statistics
    //------------------------------------------------------------------

    [[nodiscard]] uint64_t framesReceived() const { return framesReceived_; }
    [[nodiscard]] uint64_t framesDropped() const;
    [[nodiscard]] uint64_t bufferOverruns() const;
    [[nodiscard]] uint64_t lastSequence() const { return lastSequence_; }
    [[nodiscard]] uint64_t packetsReceived() const;

    // Get last received frame (for inspection)
    [[nodiscard]] const IQFrame& lastFrame() const { return lastFrame_; }

    // Reset statistics
    void resetStats();

private:
    void receiveLoop();

    std::vector<uint8_t> processResponse(const std::vector<uint8_t>& responseData, const std::string& cmd_id);

    TwinConfig config_;
    std::unique_ptr<TcpControlClient> control_;
    std::unique_ptr<UdpStreamClient> stream_;
    bool connected_ = false;

    mutable std::mutex callbackMutex_;
    FrameCallback frameCallback_;
    BatchCallback batchCallback_;

    std::thread receiveThread_;
    std::atomic<bool> receiving_{false};
    std::atomic<bool> stopRequested_{false};
    std::atomic<bool> firstFrame_{true};

    std::atomic<uint64_t> framesReceived_{0};
    std::atomic<uint64_t> lastSequence_{0};

    IQFrame lastFrame_{};
    std::vector<IQFrame> frameBuffer_;
};

} // namespace nexrx
