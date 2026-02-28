// NexRx App - Twin Connection Implementation (Cross-platform)
//
// Copyright 2026 NexRx Project - MIT License

#include "TwinConn.hpp"

#include <cbor.h>
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
            // Send GBYE command
            sendCborRequest("GBYE", {});
            if (config_.verbose) {
                std::cout << "[TwinConn] Sent GBYE to twin" << std::endl;
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

        {
            std::lock_guard<std::mutex> lock(callbackMutex_);
            if (frameCallback_) {
                frameCallback_(frame);
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (!frameBuffer_.empty() && batchCallback_) {
            batchCallback_(frameBuffer_);
        }
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
// Control Commands (via TCP/CBOR)
//------------------------------------------------------------------

std::vector<uint8_t> TwinConn::processResponse(const std::vector<uint8_t>& responseData, const std::string& cmd_id) {
    if (responseData.empty()) return {};

    // Response is [status, payload]
    CborParser resParser;
    CborValue resIt, resArray;
    if (cbor_parser_init(responseData.data(), responseData.size(), 0, &resParser, &resIt) != CborNoError) return {};
    if (!cbor_value_is_array(&resIt)) return {};
    
    cbor_value_enter_container(&resIt, &resArray);
    
    int64_t status;
    if (!cbor_value_is_integer(&resArray)) return {};
    cbor_value_get_int64(&resArray, &status);
    cbor_value_advance(&resArray);
    
    if (status != 0) {
        if (config_.verbose) std::cerr << "[TwinConn] Command " << cmd_id << " returned error " << status << std::endl;
        return {};
    }

    // Return the payload (the second element of the response array)
    if (cbor_value_is_text_string(&resArray)) {
        size_t payloadLen;
        cbor_value_get_string_length(&resArray, &payloadLen);
        std::vector<uint8_t> payload(payloadLen + 1);
        cbor_value_copy_text_string(&resArray, reinterpret_cast<char*>(payload.data()), &payloadLen, &resArray);
        payload.resize(payloadLen); // Remove null terminator from vector
        return payload;
    }
    
    return responseData; // Fallback
}

std::vector<uint8_t> TwinConn::sendCborRequest(const std::string& cmd_id, const std::vector<uint8_t>& request) {
    auto result = control_->sendRequest(request, std::chrono::milliseconds(500));
    if (!result.ok()) {
        if (config_.verbose) std::cerr << "[TwinConn] Command failed: " << cmd_id << " (timeout/error)" << std::endl;
        return {};
    }
    return processResponse(result.value, cmd_id);
}

bool TwinConn::setQsdVfo(int index, double freq_hz) {
    uint8_t buffer[128];
    CborEncoder encoder, array, args;
    cbor_encoder_init(&encoder, buffer, sizeof(buffer), 0);
    cbor_encoder_create_array(&encoder, &array, 2);
    cbor_encode_text_stringz(&array, "SVFO");
    cbor_encoder_create_array(&array, &args, 2);
    cbor_encode_uint(&args, index);
    cbor_encode_double(&args, freq_hz);
    cbor_encoder_close_container(&array, &args);
    cbor_encoder_close_container(&encoder, &array);
    
    return !sendCborRequest("SVFO", {buffer, buffer + cbor_encoder_get_buffer_size(&encoder, buffer)}).empty();
}

bool TwinConn::setAtten(int db_value, bool enabled) {
    uint8_t buffer[128];
    CborEncoder encoder, array, args;
    cbor_encoder_init(&encoder, buffer, sizeof(buffer), 0);
    cbor_encoder_create_array(&encoder, &array, 2);
    cbor_encode_text_stringz(&array, "SATT");
    cbor_encoder_create_array(&array, &args, 2);
    cbor_encode_uint(&args, db_value);
    cbor_encode_boolean(&args, enabled);
    cbor_encoder_close_container(&array, &args);
    cbor_encoder_close_container(&encoder, &array);
    
    return !sendCborRequest("SATT", {buffer, buffer + cbor_encoder_get_buffer_size(&encoder, buffer)}).empty();
}

bool TwinConn::setPreselectorCap(int index, bool enabled) {
    uint8_t buffer[128];
    CborEncoder encoder, array, args;
    cbor_encoder_init(&encoder, buffer, sizeof(buffer), 0);
    cbor_encoder_create_array(&encoder, &array, 2);
    cbor_encode_text_stringz(&array, "SPRC");
    cbor_encoder_create_array(&array, &args, 2);
    cbor_encode_uint(&args, index);
    cbor_encode_boolean(&args, enabled);
    cbor_encoder_close_container(&array, &args);
    cbor_encoder_close_container(&encoder, &array);
    
    return !sendCborRequest("SPRC", {buffer, buffer + cbor_encoder_get_buffer_size(&encoder, buffer)}).empty();
}

bool TwinConn::setPreselectorInd(int index, bool enabled) {
    uint8_t buffer[128];
    CborEncoder encoder, array, args;
    cbor_encoder_init(&encoder, buffer, sizeof(buffer), 0);
    cbor_encoder_create_array(&encoder, &array, 2);
    cbor_encode_text_stringz(&array, "SPRL");
    cbor_encoder_create_array(&array, &args, 2);
    cbor_encode_uint(&args, index);
    cbor_encode_boolean(&args, enabled);
    cbor_encoder_close_container(&array, &args);
    cbor_encoder_close_container(&encoder, &array);
    
    return !sendCborRequest("SPRL", {buffer, buffer + cbor_encoder_get_buffer_size(&encoder, buffer)}).empty();
}

bool TwinConn::setPgaGain(int index, double gain_db) {
    uint8_t buffer[128];
    CborEncoder encoder, array, args;
    cbor_encoder_init(&encoder, buffer, sizeof(buffer), 0);
    cbor_encoder_create_array(&encoder, &array, 2);
    cbor_encode_text_stringz(&array, "SPGA");
    cbor_encoder_create_array(&array, &args, 2);
    cbor_encode_uint(&args, index);
    cbor_encode_double(&args, gain_db);
    cbor_encoder_close_container(&array, &args);
    cbor_encoder_close_container(&encoder, &array);
    
    return !sendCborRequest("SPGA", {buffer, buffer + cbor_encoder_get_buffer_size(&encoder, buffer)}).empty();
}

bool TwinConn::setIsgEnable(bool enabled) {
    uint8_t buffer[128];
    CborEncoder encoder, array, args;
    cbor_encoder_init(&encoder, buffer, sizeof(buffer), 0);
    cbor_encoder_create_array(&encoder, &array, 2);
    cbor_encode_text_stringz(&array, "SIEN");
    cbor_encoder_create_array(&array, &args, 1);
    cbor_encode_boolean(&args, enabled);
    cbor_encoder_close_container(&array, &args);
    cbor_encoder_close_container(&encoder, &array);
    
    return !sendCborRequest("SIEN", {buffer, buffer + cbor_encoder_get_buffer_size(&encoder, buffer)}).empty();
}

bool TwinConn::setIsgFreq(double freq_hz) {
    uint8_t buffer[128];
    CborEncoder encoder, array, args;
    cbor_encoder_init(&encoder, buffer, sizeof(buffer), 0);
    cbor_encoder_create_array(&encoder, &array, 2);
    cbor_encode_text_stringz(&array, "SIFQ");
    cbor_encoder_create_array(&array, &args, 1);
    cbor_encode_double(&args, freq_hz);
    cbor_encoder_close_container(&array, &args);
    cbor_encoder_close_container(&encoder, &array);
    
    return !sendCborRequest("SIFQ", {buffer, buffer + cbor_encoder_get_buffer_size(&encoder, buffer)}).empty();
}

bool TwinConn::setCodecConfig(int rate, const std::vector<int>& channel_map, double gain, int filter_type) {
    uint8_t buffer[512];
    CborEncoder encoder, array, args, mapArray;
    cbor_encoder_init(&encoder, buffer, sizeof(buffer), 0);
    cbor_encoder_create_array(&encoder, &array, 2);
    cbor_encode_text_stringz(&array, "SCOD");
    cbor_encoder_create_array(&array, &args, 4);
    cbor_encode_uint(&args, rate);
    
    cbor_encoder_create_array(&args, &mapArray, channel_map.size());
    for (int ch : channel_map) cbor_encode_uint(&mapArray, ch);
    cbor_encoder_close_container(&args, &mapArray);
    
    cbor_encode_double(&args, gain);
    cbor_encode_uint(&args, filter_type);
    cbor_encoder_close_container(&array, &args);
    cbor_encoder_close_container(&encoder, &array);
    
    return !sendCborRequest("SCOD", {buffer, buffer + cbor_encoder_get_buffer_size(&encoder, buffer)}).empty();
}

bool TwinConn::setCalibration(const std::string& type, const std::string& json_data) {
    std::vector<uint8_t> buffer(json_data.size() + 256);
    CborEncoder encoder, array, args;
    cbor_encoder_init(&encoder, buffer.data(), buffer.size(), 0);
    cbor_encoder_create_array(&encoder, &array, 2);
    cbor_encode_text_stringz(&array, "SCAL");
    cbor_encoder_create_array(&array, &args, 2);
    cbor_encode_text_stringz(&args, type.c_str());
    cbor_encode_text_stringz(&args, json_data.c_str());
    cbor_encoder_close_container(&array, &args);
    cbor_encoder_close_container(&encoder, &array);
    
    return !sendCborRequest("SCAL", {buffer.data(), buffer.data() + cbor_encoder_get_buffer_size(&encoder, buffer.data())}).empty();
}

std::string TwinConn::getCalibration(const std::string& type) {
    uint8_t buffer[256];
    CborEncoder encoder, array, args;
    cbor_encoder_init(&encoder, buffer, sizeof(buffer), 0);
    cbor_encoder_create_array(&encoder, &array, 2);
    cbor_encode_text_stringz(&array, "GCAL");
    cbor_encoder_create_array(&array, &args, 1);
    cbor_encode_text_stringz(&args, type.c_str());
    cbor_encoder_close_container(&array, &args);
    cbor_encoder_close_container(&encoder, &array);
    
    auto res = sendCborRequest("GCAL", {buffer, buffer + cbor_encoder_get_buffer_size(&encoder, buffer)});
    return std::string(res.begin(), res.end());
}

std::string TwinConn::getStatus() {
    uint8_t buffer[64];
    CborEncoder encoder, array, args;
    cbor_encoder_init(&encoder, buffer, sizeof(buffer), 0);
    cbor_encoder_create_array(&encoder, &array, 2);
    cbor_encode_text_stringz(&array, "GSTS");
    cbor_encoder_create_array(&array, &args, 0);
    cbor_encoder_close_container(&array, &args);
    cbor_encoder_close_container(&encoder, &array);
    
    auto res = sendCborRequest("GSTS", {buffer, buffer + cbor_encoder_get_buffer_size(&encoder, buffer)});
    return std::string(res.begin(), res.end());
}

std::string TwinConn::getHardwareConfig() {
    uint8_t buffer[64];
    CborEncoder encoder, array, args;
    cbor_encoder_init(&encoder, buffer, sizeof(buffer), 0);
    cbor_encoder_create_array(&encoder, &array, 2);
    cbor_encode_text_stringz(&array, "GCNF");
    cbor_encoder_create_array(&array, &args, 0);
    cbor_encoder_close_container(&array, &args);
    cbor_encoder_close_container(&encoder, &array);
    
    auto res = sendCborRequest("GCNF", {buffer, buffer + cbor_encoder_get_buffer_size(&encoder, buffer)});
    return std::string(res.begin(), res.end());
}

bool TwinConn::startStream() {
    uint8_t buffer[64];
    CborEncoder encoder, array, args;
    cbor_encoder_init(&encoder, buffer, sizeof(buffer), 0);
    cbor_encoder_create_array(&encoder, &array, 2);
    cbor_encode_text_stringz(&array, "STM[");
    cbor_encoder_create_array(&array, &args, 0);
    cbor_encoder_close_container(&array, &args);
    cbor_encoder_close_container(&encoder, &array);
    
    return !sendCborRequest("STM[", {buffer, buffer + cbor_encoder_get_buffer_size(&encoder, buffer)}).empty();
}

bool TwinConn::stopStream() {
    uint8_t buffer[64];
    CborEncoder encoder, array, args;
    cbor_encoder_init(&encoder, buffer, sizeof(buffer), 0);
    cbor_encoder_create_array(&encoder, &array, 2);
    cbor_encode_text_stringz(&array, "]STM");
    cbor_encoder_create_array(&array, &args, 0);
    cbor_encoder_close_container(&array, &args);
    cbor_encoder_close_container(&encoder, &array);
    
    return !sendCborRequest("]STM", {buffer, buffer + cbor_encoder_get_buffer_size(&encoder, buffer)}).empty();
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
    firstFrame_.store(true, std::memory_order_relaxed);
    if (stream_) {
        stream_->clear();
    }
}

} // namespace nexrx
