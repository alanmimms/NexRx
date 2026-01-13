// NexRx Digital Twin - UDP Stream Transport Implementation
//
// Uses tinycbor for efficient CBOR encoding/decoding.
//
// Copyright 2026 NexRx Project - MIT License

#include "UdpStreamTransport.hpp"

#include <cbor.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include <cstring>

namespace nexrx {

UdpStreamTransport::UdpStreamTransport(const UdpStreamConfig& config)
    : config_(config)
{
    sendBuffer_.reserve(config_.framesPerPacket);
    receiveBuffer_.resize(config_.receiveBufferSize);
    std::memset(&destAddr_, 0, sizeof(destAddr_));
}

UdpStreamTransport::~UdpStreamTransport() {
    disconnect();
}

bool UdpStreamTransport::connect() {
    if (isConnected()) {
        return true;
    }

    // Create UDP socket
    socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd_ < 0) {
        return false;
    }

    // Allow address reuse
    int opt = 1;
    setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (config_.server) {
        // Sender mode: just create socket, will use sendto()
        // Set up default destination
        destAddr_.sin_family = AF_INET;
        destAddr_.sin_port = htons(config_.port);
        if (config_.host == "0.0.0.0" || config_.host.empty()) {
            destAddr_.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        } else {
            inet_pton(AF_INET, config_.host.c_str(), &destAddr_.sin_addr);
        }
        return true;
    } else {
        // Receiver mode: bind to port
        sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(config_.port);

        if (config_.host == "0.0.0.0" || config_.host.empty()) {
            addr.sin_addr.s_addr = INADDR_ANY;
        } else {
            inet_pton(AF_INET, config_.host.c_str(), &addr.sin_addr);
        }

        if (bind(socket_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            close(socket_fd_);
            socket_fd_ = -1;
            return false;
        }

        // Set receive buffer size (larger for high bandwidth)
        int rcvbuf = 1024 * 1024;  // 1MB
        setsockopt(socket_fd_, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

        // Start receiver thread
        running_ = true;
        receiveThread_ = std::thread(&UdpStreamTransport::receiveLoop, this);

        return true;
    }
}

void UdpStreamTransport::disconnect() {
    // Stop receiver thread
    if (running_) {
        running_ = false;
        if (receiveThread_.joinable()) {
            receiveThread_.join();
        }
    }

    // Flush any pending frames
    if (config_.server && !sendBuffer_.empty()) {
        flush();
    }

    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
}

bool UdpStreamTransport::isConnected() const {
    return socket_fd_ >= 0;
}

std::string UdpStreamTransport::name() const {
    if (config_.server) {
        return "UdpStream:sender:" + std::to_string(config_.port);
    } else {
        return "UdpStream:receiver:" + std::to_string(config_.port);
    }
}

void UdpStreamTransport::setDestination(const std::string& host, uint16_t port) {
    std::lock_guard<std::mutex> lock(destMutex_);
    destAddr_.sin_family = AF_INET;
    destAddr_.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &destAddr_.sin_addr);
}

TransportError UdpStreamTransport::write(const IQFrame& frame) {
    if (!config_.server) {
        return TransportError::InvalidData;
    }

    if (socket_fd_ < 0) {
        return TransportError::NotConnected;
    }

    {
        std::lock_guard<std::mutex> lock(sendMutex_);
        sendBuffer_.push_back(frame);

        if (sendBuffer_.size() >= config_.framesPerPacket) {
            if (!sendPacket()) {
                return TransportError::IoError;
            }
        }
    }

    return TransportError::None;
}

TransportError UdpStreamTransport::writeBatch(std::span<const IQFrame> frameBatch) {
    if (!config_.server) {
        return TransportError::InvalidData;
    }

    if (socket_fd_ < 0) {
        return TransportError::NotConnected;
    }

    // Single lock for entire batch - much faster than per-frame locking
    std::lock_guard<std::mutex> lock(sendMutex_);

    for (const auto& frame : frameBatch) {
        sendBuffer_.push_back(frame);

        if (sendBuffer_.size() >= config_.framesPerPacket) {
            if (!sendPacket()) {
                return TransportError::IoError;
            }
        }
    }

    return TransportError::None;
}

void UdpStreamTransport::flush() {
    if (!config_.server || sendBuffer_.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(sendMutex_);
    sendPacket();
}

bool UdpStreamTransport::sendPacket() {
    if (sendBuffer_.empty()) {
        return true;
    }

    // CBOR packet format: [magic, version, type, [[frame0], [frame1], ...]]
    // Max size: header (~20 bytes) + frames (32 * ~50 bytes) = ~1700 bytes
    uint8_t buffer[2048];
    CborEncoder encoder;
    cbor_encoder_init(&encoder, buffer, sizeof(buffer), 0);

    // Create outer array [magic, version, type, frames]
    CborEncoder packetArray;
    cbor_encoder_create_array(&encoder, &packetArray, 4);

    // Magic string
    cbor_encode_text_stringz(&packetArray, UdpProtocol::MAGIC);

    // Version
    cbor_encode_uint(&packetArray, UdpProtocol::VERSION);

    // Type
    cbor_encode_uint(&packetArray, UdpProtocol::TYPE_IQ_DATA);

    // Frames array
    CborEncoder framesArray;
    cbor_encoder_create_array(&packetArray, &framesArray, sendBuffer_.size());

    for (const auto& f : sendBuffer_) {
        // Each frame: [seq, ts_ns, i0, q0, i1, q1, i2, q2]
        CborEncoder frameArray;
        cbor_encoder_create_array(&framesArray, &frameArray, 8);

        cbor_encode_uint(&frameArray, f.sequence);
        cbor_encode_uint(&frameArray, f.timestamp_ns);
        cbor_encode_int(&frameArray, f.qsd[0].i);
        cbor_encode_int(&frameArray, f.qsd[0].q);
        cbor_encode_int(&frameArray, f.qsd[1].i);
        cbor_encode_int(&frameArray, f.qsd[1].q);
        cbor_encode_int(&frameArray, f.qsd[2].i);
        cbor_encode_int(&frameArray, f.qsd[2].q);

        cbor_encoder_close_container(&framesArray, &frameArray);
    }

    cbor_encoder_close_container(&packetArray, &framesArray);
    cbor_encoder_close_container(&encoder, &packetArray);

    size_t len = cbor_encoder_get_buffer_size(&encoder, buffer);

    // Send
    ssize_t sent;
    {
        std::lock_guard<std::mutex> lock(destMutex_);
        sent = sendto(socket_fd_, buffer, len, 0,
                      reinterpret_cast<sockaddr*>(&destAddr_),
                      sizeof(destAddr_));
    }

    if (sent < 0) {
        return false;
    }

    framesSent_.fetch_add(sendBuffer_.size(), std::memory_order_relaxed);
    packetsSent_.fetch_add(1, std::memory_order_relaxed);
    sendBuffer_.clear();

    return true;
}

void UdpStreamTransport::receiveLoop() {
    constexpr size_t MAX_PACKET_SIZE = 4096;
    uint8_t buffer[MAX_PACKET_SIZE];

    while (running_) {
        // Poll with timeout so we can check running_ flag
        struct pollfd pfd;
        pfd.fd = socket_fd_;
        pfd.events = POLLIN;

        int ret = poll(&pfd, 1, 100);  // 100ms timeout
        if (ret <= 0) {
            continue;
        }

        ssize_t received = recvfrom(socket_fd_, buffer, sizeof(buffer), 0, nullptr, nullptr);
        if (received <= 0) {
            continue;
        }

        // Parse CBOR packet
        CborParser parser;
        CborValue it;
        if (cbor_parser_init(buffer, received, 0, &parser, &it) != CborNoError) {
            continue;
        }

        // Expect outer array
        if (!cbor_value_is_array(&it)) {
            continue;
        }

        CborValue packetArray;
        if (cbor_value_enter_container(&it, &packetArray) != CborNoError) {
            continue;
        }

        // Magic string
        if (!cbor_value_is_text_string(&packetArray)) {
            continue;
        }
        char magic[8];
        size_t magic_len = sizeof(magic);
        if (cbor_value_copy_text_string(&packetArray, magic, &magic_len, &packetArray) != CborNoError) {
            continue;
        }
        if (strcmp(magic, UdpProtocol::MAGIC) != 0) {
            continue;
        }

        // Version
        if (!cbor_value_is_unsigned_integer(&packetArray)) {
            continue;
        }
        uint64_t version;
        cbor_value_get_uint64(&packetArray, &version);
        cbor_value_advance(&packetArray);
        if (version != UdpProtocol::VERSION) {
            continue;
        }

        // Type
        if (!cbor_value_is_unsigned_integer(&packetArray)) {
            continue;
        }
        uint64_t type;
        cbor_value_get_uint64(&packetArray, &type);
        cbor_value_advance(&packetArray);
        if (type != UdpProtocol::TYPE_IQ_DATA) {
            continue;  // Not I/Q data (might be hole punch)
        }

        // Frames array
        if (!cbor_value_is_array(&packetArray)) {
            continue;
        }

        CborValue framesArray;
        if (cbor_value_enter_container(&packetArray, &framesArray) != CborNoError) {
            continue;
        }

        packetsReceived_.fetch_add(1, std::memory_order_relaxed);

        // Process frames
        while (!cbor_value_at_end(&framesArray)) {
            if (!cbor_value_is_array(&framesArray)) {
                cbor_value_advance(&framesArray);
                continue;
            }

            CborValue frameArray;
            if (cbor_value_enter_container(&framesArray, &frameArray) != CborNoError) {
                cbor_value_advance(&framesArray);
                continue;
            }

            IQFrame frame;
            uint64_t u64val;
            int64_t i64val;

            // sequence
            if (!cbor_value_is_unsigned_integer(&frameArray)) goto next_frame;
            cbor_value_get_uint64(&frameArray, &u64val);
            frame.sequence = static_cast<uint32_t>(u64val);
            cbor_value_advance(&frameArray);

            // timestamp_ns
            if (!cbor_value_is_unsigned_integer(&frameArray)) goto next_frame;
            cbor_value_get_uint64(&frameArray, &u64val);
            frame.timestamp_ns = u64val;
            cbor_value_advance(&frameArray);

            // i0, q0, i1, q1, i2, q2
            for (int ch = 0; ch < 3; ++ch) {
                // i
                if (!cbor_value_is_integer(&frameArray)) goto next_frame;
                cbor_value_get_int64(&frameArray, &i64val);
                frame.qsd[ch].i = static_cast<int32_t>(i64val);
                cbor_value_advance(&frameArray);

                // q
                if (!cbor_value_is_integer(&frameArray)) goto next_frame;
                cbor_value_get_int64(&frameArray, &i64val);
                frame.qsd[ch].q = static_cast<int32_t>(i64val);
                cbor_value_advance(&frameArray);
            }

            frame.flags = 0;

            // Check for dropped frames via sequence number
            if (!firstFrame_.load(std::memory_order_relaxed)) {
                uint32_t expected = lastSequence_.load(std::memory_order_relaxed) + 1;
                if (frame.sequence != expected) {
                    uint32_t dropped = frame.sequence - expected;
                    framesDropped_.fetch_add(dropped, std::memory_order_relaxed);
                }
            } else {
                firstFrame_.store(false, std::memory_order_relaxed);
            }
            lastSequence_.store(frame.sequence, std::memory_order_relaxed);

            // Write to ring buffer
            {
                size_t writePos = writePos_.load(std::memory_order_relaxed);
                size_t readPos = readPos_.load(std::memory_order_acquire);

                // Check for buffer full
                size_t nextWrite = (writePos + 1) % config_.receiveBufferSize;
                if (nextWrite == readPos) {
                    bufferOverruns_.fetch_add(1, std::memory_order_relaxed);
                    goto next_frame;
                }

                receiveBuffer_[writePos] = frame;
                writePos_.store(nextWrite, std::memory_order_release);
                framesReceived_.fetch_add(1, std::memory_order_relaxed);
            }

        next_frame:
            cbor_value_leave_container(&framesArray, &frameArray);
        }

        cbor_value_leave_container(&packetArray, &framesArray);
    }
}

Result<IQFrame> UdpStreamTransport::read(std::chrono::milliseconds timeout) {
    if (config_.server) {
        return {IQFrame{}, TransportError::InvalidData};
    }

    if (socket_fd_ < 0) {
        return {IQFrame{}, TransportError::NotConnected};
    }

    auto deadline = std::chrono::steady_clock::now() + timeout;

    // Always check at least once (handles timeout=0 case)
    do {
        size_t readPos = readPos_.load(std::memory_order_relaxed);
        size_t writePos = writePos_.load(std::memory_order_acquire);

        if (readPos != writePos) {
            IQFrame frame = receiveBuffer_[readPos];
            readPos_.store((readPos + 1) % config_.receiveBufferSize,
                          std::memory_order_release);
            return {frame, TransportError::None};
        }

        // For non-zero timeout, sleep briefly before retry
        if (timeout.count() > 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    } while (timeout.count() > 0 && std::chrono::steady_clock::now() < deadline);

    return {IQFrame{}, TransportError::Timeout};
}

Result<std::vector<IQFrame>> UdpStreamTransport::readBatch(
    size_t maxFrames,
    std::chrono::milliseconds timeout
) {
    if (config_.server) {
        return {{}, TransportError::InvalidData};
    }

    if (socket_fd_ < 0) {
        return {{}, TransportError::NotConnected};
    }

    std::vector<IQFrame> frames;
    frames.reserve(maxFrames);

    auto deadline = std::chrono::steady_clock::now() + timeout;

    // Always check at least once (handles timeout=0 case)
    do {
        size_t readPos = readPos_.load(std::memory_order_relaxed);
        size_t writePos = writePos_.load(std::memory_order_acquire);

        if (readPos != writePos) {
            frames.push_back(receiveBuffer_[readPos]);
            readPos_.store((readPos + 1) % config_.receiveBufferSize,
                          std::memory_order_release);
        } else if (frames.empty() && timeout.count() > 0) {
            // No frames yet, wait a bit
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        } else if (!frames.empty()) {
            // Got some frames, return what we have
            break;
        } else {
            // timeout=0 and no frames, exit immediately
            break;
        }
    } while (frames.size() < maxFrames &&
             (timeout.count() == 0 || std::chrono::steady_clock::now() < deadline));

    if (frames.empty()) {
        return {{}, TransportError::Timeout};
    }

    return {std::move(frames), TransportError::None};
}

size_t UdpStreamTransport::available() const {
    size_t readPos = readPos_.load(std::memory_order_relaxed);
    size_t writePos = writePos_.load(std::memory_order_acquire);

    if (writePos >= readPos) {
        return writePos - readPos;
    } else {
        return config_.receiveBufferSize - readPos + writePos;
    }
}

size_t UdpStreamTransport::capacity() const {
    return config_.receiveBufferSize;
}

void UdpStreamTransport::clear() {
    readPos_.store(writePos_.load(std::memory_order_acquire),
                   std::memory_order_release);
}

} // namespace nexrx
