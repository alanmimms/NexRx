// NexRx Digital Twin - Host Application Implementation
//
// Copyright 2026 NexRx Project - MIT License

#include "HostApp.hpp"

#include <iostream>
#include <sstream>
#include <chrono>

namespace nexrx {

HostApp::~HostApp() {
    shutdown();
}

bool HostApp::initialize(const HostConfig& config) {
    if (connected_) {
        return true;
    }

    config_ = config;
    frameBuffer_.reserve(config.frameBufferSize);

    // Create TCP control transport (client mode)
    TcpControlConfig ctlConfig;
    ctlConfig.host = config.host;
    ctlConfig.port = config.controlPort;
    ctlConfig.server = false;

    control_ = std::make_unique<TcpControlTransport>(ctlConfig);

    if (!control_->connect()) {
        if (config.verbose) {
            std::cerr << "[HostApp] Failed to connect to TCP control: "
                      << config.host << ":" << config.controlPort << std::endl;
        }
        control_.reset();
        return false;
    }

    if (config.verbose) {
        std::cout << "[HostApp] Connected to TCP control: "
                  << config.host << ":" << config.controlPort << std::endl;
    }

    // Create UDP stream transport (receiver mode)
    UdpStreamConfig streamConfig;
    streamConfig.host = "0.0.0.0";  // Bind to all interfaces
    streamConfig.port = config.streamPort;
    streamConfig.server = false;    // Receiver mode
    streamConfig.receiveBufferSize = config.receiveBufferSize;

    stream_ = std::make_unique<UdpStreamTransport>(streamConfig);

    if (!stream_->connect()) {
        if (config.verbose) {
            std::cerr << "[HostApp] Failed to bind UDP stream port: "
                      << config.streamPort << std::endl;
        }
        control_->disconnect();
        control_.reset();
        stream_.reset();
        return false;
    }

    if (config.verbose) {
        std::cout << "[HostApp] Listening on UDP port: "
                  << config.streamPort << std::endl;
    }

    connected_ = true;
    return true;
}

void HostApp::shutdown() {
    stopReceiving();

    if (control_) {
        control_->disconnect();
        control_.reset();
    }

    if (stream_) {
        stream_->disconnect();
        stream_.reset();
    }

    connected_ = false;
}

bool HostApp::startReceiving() {
    if (!connected_ || receiving_) {
        return false;
    }

    stopRequested_ = false;
    receiving_ = true;

    receiveThread_ = std::thread(&HostApp::receiveLoop, this);

    if (config_.verbose) {
        std::cout << "[HostApp] Started receiving" << std::endl;
    }

    return true;
}

void HostApp::stopReceiving() {
    if (!receiving_) {
        return;
    }

    stopRequested_ = true;

    if (receiveThread_.joinable()) {
        receiveThread_.join();
    }

    receiving_ = false;

    if (config_.verbose) {
        std::cout << "[HostApp] Stopped receiving" << std::endl;
    }
}

size_t HostApp::pollFrames(size_t maxFrames) {
    if (!connected_ || !stream_) {
        return 0;
    }

    frameBuffer_.clear();
    size_t count = 0;

    while (count < maxFrames) {
        // Non-blocking read with zero timeout
        auto result = stream_->read(std::chrono::milliseconds(0));
        if (!result.ok()) {
            break;  // No more frames available
        }

        IQFrame frame = result.value;

        lastSequence_ = frame.sequence;
        lastFrame_ = frame;
        ++framesReceived_;
        ++count;

        frameBuffer_.push_back(frame);

        if (frameCallback_) {
            frameCallback_(frame);
        }
    }

    if (!frameBuffer_.empty() && batchCallback_) {
        batchCallback_(frameBuffer_);
    }

    return count;
}

void HostApp::receiveLoop() {
    constexpr auto pollInterval = std::chrono::microseconds(100);

    while (!stopRequested_) {
        size_t received = pollFrames(100);

        if (received == 0) {
            // No frames available, sleep briefly
            std::this_thread::sleep_for(pollInterval);
        }
    }
}

//------------------------------------------------------------------
// Control Commands
//------------------------------------------------------------------

std::string HostApp::sendCommand(const std::string& cmd) {
    if (!connected_ || !control_) {
        return "ERROR not connected";
    }

    std::vector<uint8_t> request(cmd.begin(), cmd.end());
    auto result = control_->sendRequest(request, std::chrono::milliseconds(1000));

    if (!result.ok()) {
        return "ERROR timeout";
    }

    return std::string(result.value.begin(), result.value.end());
}

bool HostApp::setLO(double freq_hz) {
    std::ostringstream cmd;
    cmd << "SET_LO " << std::fixed << freq_hz << "\n";

    std::string response = sendCommand(cmd.str());
    return response.find("OK") == 0;
}

bool HostApp::getStatus(double& lo_freq_hz, bool& streaming) {
    std::string response = sendCommand("GET_STATUS\n");

    if (response.find("STATUS") != 0) {
        return false;
    }

    // Parse: "STATUS lo=14200000.0 streaming=true"
    lo_freq_hz = 0.0;
    streaming = false;

    size_t loPos = response.find("lo=");
    if (loPos != std::string::npos) {
        lo_freq_hz = std::stod(response.substr(loPos + 3));
    }

    streaming = response.find("streaming=true") != std::string::npos;
    return true;
}

bool HostApp::startStream() {
    std::string response = sendCommand("START_STREAM\n");
    return response.find("OK") == 0;
}

bool HostApp::stopStream() {
    std::string response = sendCommand("STOP_STREAM\n");
    return response.find("OK") == 0;
}

//------------------------------------------------------------------
// Statistics
//------------------------------------------------------------------

uint64_t HostApp::framesDropped() const {
    return stream_ ? stream_->framesDropped() : 0;
}

uint64_t HostApp::packetsReceived() const {
    return stream_ ? stream_->packetsReceived() : 0;
}

void HostApp::resetStats() {
    framesReceived_ = 0;
    lastSequence_ = 0;
    if (stream_) {
        stream_->clear();
    }
}

} // namespace nexrx
