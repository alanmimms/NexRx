// NexRx App - Twin Connection Implementation (Cross-platform)
//
// Copyright 2026 NexRx Project - MIT License

#include "TwinConn.hpp"

#include <iostream>
#include <sstream>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace {

// Boost thread priority for real-time audio processing
void boostThreadPriority() {
#ifdef _WIN32
    // Windows: set thread to high priority
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
#else
    // POSIX: try to set real-time scheduling (may require privileges)
    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_FIFO) / 2;
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) != 0) {
        // Fall back to just nice value if SCHED_FIFO not available
        // (requires root or CAP_SYS_NICE)
    }
#endif
}

} // anonymous namespace

namespace nexrx {

TwinConn::~TwinConn() {
    shutdown();
}

bool TwinConn::initialize(const TwinConfig& config) {
    if (connected_) {
        return true;
    }

    config_ = config;
    frameBuffer_.reserve(config.frameBufferSize);

    // Create TCP control client
    TcpControlClientConfig ctlConfig;
    ctlConfig.host = config.host;
    ctlConfig.port = config.controlPort;

    control_ = std::make_unique<TcpControlClient>(ctlConfig);

    if (!control_->connect()) {
        if (config.verbose) {
            std::cerr << "[TwinConn] Failed to connect to TCP control: "
                      << config.host << ":" << config.controlPort << std::endl;
        }
        control_.reset();
        return false;
    }

    if (config.verbose) {
        std::cout << "[TwinConn] Connected to TCP control: "
                  << config.host << ":" << config.controlPort << std::endl;
    }

    // Create UDP stream client (receiver)
    UdpStreamClientConfig streamConfig;
    streamConfig.port = config.streamPort;
    streamConfig.receiveBufferSize = config.receiveBufferSize;
    streamConfig.serverHost = config.host;      // For NAT hole punch
    streamConfig.serverPort = config.streamPort; // Server listens on same port

    stream_ = std::make_unique<UdpStreamClient>(streamConfig);

    if (!stream_->connect()) {
        if (config.verbose) {
            std::cerr << "[TwinConn] Failed to bind UDP stream port: "
                      << config.streamPort << std::endl;
        }
        control_->disconnect();
        control_.reset();
        stream_.reset();
        return false;
    }

    // Send NAT hole punch to establish return path for UDP
    if (stream_->sendHolePunch()) {
        if (config.verbose) {
            std::cout << "[TwinConn] Sent UDP hole punch to "
                      << config.host << ":" << config.streamPort << std::endl;
        }
    }

    if (config.verbose) {
        std::cout << "[TwinConn] Listening on UDP port: "
                  << config.streamPort << std::endl;
    }

    connected_ = true;
    return true;
}

void TwinConn::shutdown() {
    stopReceiving();

    // Send graceful disconnect to twin before closing TCP
    if (control_ && connected_) {
        try {
            // Send DISCONNECT command - don't wait too long for response
            std::string cmd = "DISCONNECT\n";
            std::vector<uint8_t> request(cmd.begin(), cmd.end());
            control_->sendRequest(request, std::chrono::milliseconds(100));
            if (config_.verbose) {
                std::cout << "[TwinConn] Sent DISCONNECT to twin" << std::endl;
            }
        } catch (...) {
            // Ignore errors during shutdown
        }
        control_->disconnect();
        control_.reset();
    }

    if (stream_) {
        stream_->disconnect();
        stream_.reset();
    }

    connected_ = false;
}

bool TwinConn::startReceiving() {
    if (!connected_ || receiving_) {
        return false;
    }

    stopRequested_ = false;
    receiving_ = true;

    receiveThread_ = std::thread(&TwinConn::receiveLoop, this);

    if (config_.verbose) {
        std::cout << "[TwinConn] Started receiving" << std::endl;
    }

    return true;
}

void TwinConn::stopReceiving() {
    if (!receiving_) {
        return;
    }

    stopRequested_ = true;

    if (receiveThread_.joinable()) {
        receiveThread_.join();
    }

    receiving_ = false;

    if (config_.verbose) {
        std::cout << "[TwinConn] Stopped receiving" << std::endl;
    }
}

size_t TwinConn::pollFrames(size_t maxFrames) {
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

void TwinConn::receiveLoop() {
    // Boost thread priority for real-time audio processing
    boostThreadPriority();

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

std::string TwinConn::sendCommand(const std::string& cmd) {
    if (!connected_ || !control_) {
        return "ERROR not connected";
    }

    std::vector<uint8_t> request(cmd.begin(), cmd.end());
    auto result = control_->sendRequest(request, std::chrono::milliseconds(100));

    if (!result.ok()) {
        return "ERROR timeout";
    }

    return std::string(result.value.begin(), result.value.end());
}

bool TwinConn::setLO(double freq_hz) {
    std::ostringstream cmd;
    cmd << "SET_QSD_VFO 2 " << std::fixed << freq_hz << "\n";

    std::string response = sendCommand(cmd.str());
    return response.find("OK") == 0;
}

bool TwinConn::setQsdVfo(int index, double freq_hz) {
    std::ostringstream cmd;
    cmd << "SET_QSD_VFO " << index << " " << std::fixed << freq_hz << "\n";

    std::string response = sendCommand(cmd.str());
    return response.find("OK") == 0;
}

bool TwinConn::setPreselectorCap(int index, bool enabled) {
    std::ostringstream cmd;
    cmd << "SET_PRESEL_C " << index << " " << (enabled ? "1" : "0") << "\n";
    std::string response = sendCommand(cmd.str());
    return response.find("OK") == 0;
}

bool TwinConn::setPreselectorInd(bool enabled) {
    std::ostringstream cmd;
    cmd << "SET_PRESEL_L " << (enabled ? "1" : "0") << "\n";
    std::string response = sendCommand(cmd.str());
    return response.find("OK") == 0;
}

bool TwinConn::setBistEnable(bool enabled) {
    std::ostringstream cmd;
    cmd << "SET_BIST_ENABLE " << (enabled ? "1" : "0") << "\n";
    std::string response = sendCommand(cmd.str());
    return response.find("OK") == 0;
}

bool TwinConn::setBistFreq(double freq_hz) {
    std::ostringstream cmd;
    cmd << "SET_BIST_FREQ " << std::fixed << freq_hz << "\n";
    std::string response = sendCommand(cmd.str());
    return response.find("OK") == 0;
}

bool TwinConn::setCalibration(const std::string& type, const std::string& data) {
    std::string cmd = "SET_CALIBRATION " + type + " " + data + "\n";
    std::string response = sendCommand(cmd);
    return response.find("OK") == 0;
}

std::string TwinConn::getCalibration(const std::string& type) {
    std::string cmd = "GET_CALIBRATION " + type + "\n";
    return sendCommand(cmd);
}

bool TwinConn::getStatus(double& lo_freq_hz, bool& streaming) {
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

bool TwinConn::startStream() {
    std::string response = sendCommand("START_STREAM\n");
    return response.find("OK") == 0;
}

bool TwinConn::stopStream() {
    std::string response = sendCommand("STOP_STREAM\n");
    return response.find("OK") == 0;
}

//------------------------------------------------------------------
// Statistics
//------------------------------------------------------------------

uint64_t TwinConn::framesDropped() const {
    return stream_ ? stream_->framesDropped() : 0;
}

uint64_t TwinConn::bufferOverruns() const {
    return stream_ ? stream_->bufferOverruns() : 0;
}

uint64_t TwinConn::packetsReceived() const {
    return stream_ ? stream_->packetsReceived() : 0;
}

void TwinConn::resetStats() {
    framesReceived_ = 0;
    lastSequence_ = 0;
    if (stream_) {
        stream_->clear();
    }
}

} // namespace nexrx
