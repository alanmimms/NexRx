// NexRx Digital Twin - Host Application Implementation
//
// Copyright 2026 NexRx Project - MIT License

#include "HostApp.hpp"

#include <iostream>
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

    // Create transport configuration (consumer mode)
    SharedMemConfig shmConfig;
    shmConfig.name = config.shmName;
    shmConfig.create = false;  // Consumer mode - don't create, just open

    // Create transport and connect
    transport_ = std::make_unique<SharedMemTransport>(shmConfig);

    if (!transport_->connect()) {
        if (config.verbose) {
            std::cerr << "[HostApp] Failed to connect to shared memory: "
                      << config.shmName << std::endl;
        }
        transport_.reset();
        return false;
    }

    connected_ = true;

    if (config.verbose) {
        std::cout << "[HostApp] Connected to: " << config.shmName << std::endl;
    }

    return true;
}

void HostApp::shutdown() {
    stopReceiving();

    if (transport_) {
        transport_->disconnect();
        transport_.reset();
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
    if (!connected_ || !transport_) {
        return 0;
    }

    frameBuffer_.clear();
    size_t count = 0;

    while (count < maxFrames) {
        // Non-blocking read with zero timeout
        auto result = transport_->read(std::chrono::milliseconds(0));
        if (!result.ok()) {
            break;  // No more frames available
        }

        IQFrame frame = result.value;

        // Check for dropped frames
        if (framesReceived_ > 0 && frame.sequence != lastSequence_ + 1) {
            uint64_t dropped = frame.sequence - lastSequence_ - 1;
            framesDropped_ += dropped;
        }

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

void HostApp::resetStats() {
    framesReceived_ = 0;
    framesDropped_ = 0;
    lastSequence_ = 0;
}

} // namespace nexrx
