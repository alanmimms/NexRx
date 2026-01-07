// NexRx Digital Twin - Host Application
//
// Host-side application to receive and process I/Q data
// from the digital twin simulation via UDP streaming.
// Control commands sent via TCP.
//
// Copyright 2026 NexRx Project - MIT License

#pragma once

#include "transport/TcpControlTransport.hpp"
#include "transport/UdpStreamTransport.hpp"
#include "transport/IQFrame.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace nexrx {

//======================================================================
// Host Application Configuration
//======================================================================
struct HostConfig {
    std::string host = "127.0.0.1";       // Twin address
    uint16_t controlPort = 5000;           // TCP control port
    uint16_t streamPort = 5001;            // UDP stream port
    size_t frameBufferSize = 1024;         // Internal frame buffer
    size_t receiveBufferSize = 8192;       // UDP receive buffer
    bool verbose = false;
};

//======================================================================
// Host Application
//
// Connects to the digital twin's network transport and
// receives I/Q frames for processing.
//======================================================================
class HostApp {
public:
    using FrameCallback = std::function<void(const IQFrame&)>;
    using BatchCallback = std::function<void(const std::vector<IQFrame>&)>;

    HostApp() = default;
    ~HostApp();

    // Non-copyable
    HostApp(const HostApp&) = delete;
    HostApp& operator=(const HostApp&) = delete;

    // Initialize and connect to twin
    bool initialize(const HostConfig& config = HostConfig{});

    // Shutdown and disconnect
    void shutdown();

    // Check if connected
    [[nodiscard]] bool isConnected() const { return connected_; }

    // Set callback for each received frame
    void setFrameCallback(FrameCallback callback) {
        frameCallback_ = std::move(callback);
    }

    // Set callback for batch of frames (more efficient)
    void setBatchCallback(BatchCallback callback) {
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
    // Control Commands (via TCP)
    //------------------------------------------------------------------

    // Set local oscillator frequency
    bool setLO(double freq_hz);

    // Get twin status
    bool getStatus(double& lo_freq_hz, bool& streaming);

    // Start/stop streaming (usually auto-started)
    bool startStream();
    bool stopStream();

    // Send raw command (returns response)
    std::string sendCommand(const std::string& cmd);

    //------------------------------------------------------------------
    // Statistics
    //------------------------------------------------------------------

    [[nodiscard]] uint64_t framesReceived() const { return framesReceived_; }
    [[nodiscard]] uint64_t framesDropped() const;
    [[nodiscard]] uint64_t lastSequence() const { return lastSequence_; }
    [[nodiscard]] uint64_t packetsReceived() const;

    // Get last received frame (for inspection)
    [[nodiscard]] const IQFrame& lastFrame() const { return lastFrame_; }

    // Reset statistics
    void resetStats();

private:
    void receiveLoop();

    HostConfig config_;
    std::unique_ptr<TcpControlTransport> control_;
    std::unique_ptr<UdpStreamTransport> stream_;
    bool connected_ = false;

    FrameCallback frameCallback_;
    BatchCallback batchCallback_;

    std::thread receiveThread_;
    std::atomic<bool> receiving_{false};
    std::atomic<bool> stopRequested_{false};

    std::atomic<uint64_t> framesReceived_{0};
    std::atomic<uint64_t> lastSequence_{0};

    IQFrame lastFrame_{};
    std::vector<IQFrame> frameBuffer_;
};

} // namespace nexrx
