// NexRx App - UDP Stream Client Implementation
//
// Uses CBOR encoding for cross-platform compatibility.
//
// Copyright 2026 NexRx Project - MIT License

#include "UdpStreamClient.hpp"

#include <nlohmann/json.hpp>

#include <cstring>
#include <algorithm>

using json = nlohmann::json;

namespace nexrx {

UdpStreamClient::UdpStreamClient(const UdpStreamClientConfig& config)
    : config_(config)
{
    receiveBuffer_.resize(config_.receiveBufferSize);
}

UdpStreamClient::~UdpStreamClient() {
    disconnect();
}

bool UdpStreamClient::connect() {
    if (isConnected()) {
        return true;
    }

    fprintf(stderr, "[UDP] Creating UDP socket for port %d...\n", config_.port);
    fflush(stderr);

    // Create UDP socket
    socket_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_ == SOCKET_INVALID) {
        fprintf(stderr, "[UDP] Failed to create socket\n");
        fflush(stderr);
        return false;
    }

    fprintf(stderr, "[UDP] Socket created: fd=%d\n", (int)socket_);
    fflush(stderr);

    // Allow address reuse
    net::setReuseAddr(socket_, true);

    // Bind to receive port
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config_.port);
    addr.sin_addr.s_addr = INADDR_ANY;

    fprintf(stderr, "[UDP] Binding to 0.0.0.0:%d...\n", config_.port);
    fflush(stderr);

    if (bind(socket_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        fprintf(stderr, "[UDP] Bind failed! errno=%d\n", errno);
        fflush(stderr);
        socket_close(socket_);
        socket_ = SOCKET_INVALID;
        return false;
    }

    // Get actual bound address for logging
    sockaddr_in boundAddr;
    socklen_t boundLen = sizeof(boundAddr);
    if (getsockname(socket_, reinterpret_cast<struct sockaddr*>(&boundAddr), &boundLen) == 0) {
        char boundIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &boundAddr.sin_addr, boundIp, sizeof(boundIp));
        fprintf(stderr, "[UDP] Successfully bound to %s:%d\n", boundIp, ntohs(boundAddr.sin_port));
        fflush(stderr);
    }

    // Reset state
    writePos_.store(0, std::memory_order_relaxed);
    readPos_.store(0, std::memory_order_relaxed);
    lastSequence_.store(0, std::memory_order_relaxed);
    firstFrame_.store(true, std::memory_order_relaxed);
    packetsReceived_.store(0, std::memory_order_relaxed);
    framesReceived_.store(0, std::memory_order_relaxed);
    framesDropped_.store(0, std::memory_order_relaxed);
    bufferOverruns_.store(0, std::memory_order_relaxed);

    // Start receive thread
    running_.store(true, std::memory_order_release);
    receiveThread_ = std::thread(&UdpStreamClient::receiveLoop, this);

    return true;
}

void UdpStreamClient::disconnect() {
    // Stop receive thread
    running_.store(false, std::memory_order_release);
    if (receiveThread_.joinable()) {
        receiveThread_.join();
    }

    // Close socket
    if (socket_ != SOCKET_INVALID) {
        socket_close(socket_);
        socket_ = SOCKET_INVALID;
    }
}

bool UdpStreamClient::isConnected() const {
    return socket_ != SOCKET_INVALID;
}

std::string UdpStreamClient::name() const {
    return "UdpStreamClient:" + std::to_string(config_.port);
}

TransportError UdpStreamClient::write(const IQFrame& /*frame*/) {
    // Client doesn't write
    return TransportError::InvalidData;
}

TransportError UdpStreamClient::writeBatch(std::span<const IQFrame> /*frames*/) {
    // Client doesn't write
    return TransportError::InvalidData;
}

Result<IQFrame> UdpStreamClient::read(std::chrono::milliseconds timeout) {
    auto deadline = std::chrono::steady_clock::now() + timeout;

    do {
        size_t readPos = readPos_.load(std::memory_order_relaxed);
        size_t writePos = writePos_.load(std::memory_order_acquire);

        if (readPos != writePos) {
            IQFrame frame = receiveBuffer_[readPos];
            readPos_.store((readPos + 1) % config_.receiveBufferSize,
                          std::memory_order_release);
            return {frame, TransportError::None};
        }

        if (timeout.count() > 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    } while (timeout.count() > 0 && std::chrono::steady_clock::now() < deadline);

    // Check one more time for timeout=0 case
    size_t readPos = readPos_.load(std::memory_order_relaxed);
    size_t writePos = writePos_.load(std::memory_order_acquire);
    if (readPos != writePos) {
        IQFrame frame = receiveBuffer_[readPos];
        readPos_.store((readPos + 1) % config_.receiveBufferSize,
                      std::memory_order_release);
        return {frame, TransportError::None};
    }

    return {{}, TransportError::BufferEmpty};
}

Result<std::vector<IQFrame>> UdpStreamClient::readBatch(
    size_t maxFrames,
    std::chrono::milliseconds timeout
) {
    std::vector<IQFrame> frames;
    frames.reserve(maxFrames);

    auto deadline = std::chrono::steady_clock::now() + timeout;

    while (frames.size() < maxFrames) {
        size_t readPos = readPos_.load(std::memory_order_relaxed);
        size_t writePos = writePos_.load(std::memory_order_acquire);

        if (readPos != writePos) {
            frames.push_back(receiveBuffer_[readPos]);
            readPos_.store((readPos + 1) % config_.receiveBufferSize,
                          std::memory_order_release);
        } else if (timeout.count() == 0 || std::chrono::steady_clock::now() >= deadline) {
            break;
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }

    if (frames.empty()) {
        return {{}, TransportError::BufferEmpty};
    }

    return {std::move(frames), TransportError::None};
}

size_t UdpStreamClient::available() const {
    size_t writePos = writePos_.load(std::memory_order_acquire);
    size_t readPos = readPos_.load(std::memory_order_relaxed);

    if (writePos >= readPos) {
        return writePos - readPos;
    } else {
        return config_.receiveBufferSize - readPos + writePos;
    }
}

size_t UdpStreamClient::capacity() const {
    return config_.receiveBufferSize;
}

void UdpStreamClient::clear() {
    readPos_.store(writePos_.load(std::memory_order_acquire),
                   std::memory_order_release);
}

bool UdpStreamClient::sendHolePunch() {
    if (socket_ == SOCKET_INVALID || config_.serverHost.empty()) {
        return false;
    }

    // Set up server address
    sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(config_.serverPort);

    if (inet_pton(AF_INET, config_.serverHost.c_str(), &serverAddr.sin_addr) != 1) {
        return false;
    }

    // Send a small "hello" packet to punch through NAT
    // This creates the NAT mapping so incoming packets can reach us
    const char holePunch[] = "NXRQ_HOLE_PUNCH";
    int sent = sendto(socket_,
                      holePunch,
                      sizeof(holePunch),
                      0,
                      reinterpret_cast<struct sockaddr*>(&serverAddr),
                      sizeof(serverAddr));

    return sent > 0;
}

void UdpStreamClient::receiveLoop() {
    // CBOR packets can vary in size, allocate generous buffer
    constexpr size_t MAX_PACKET_SIZE = 64 * 1024;  // 64KB should be plenty
    std::vector<uint8_t> buffer(MAX_PACKET_SIZE);

    // Debug: log that receive loop has started
    fprintf(stderr, "[UDP] Receive loop started (CBOR), listening on port %d\n", config_.port);
    fflush(stderr);

    uint64_t pollCount = 0;
    uint64_t pollTimeouts = 0;
    uint64_t pollErrors = 0;

    while (running_.load(std::memory_order_acquire)) {
        // Poll for data with timeout
        pollfd_t pfd;
        pfd.fd = socket_;
        pfd.events = POLLIN;
        pfd.revents = 0;

        int ret = socket_poll(&pfd, 1, 100);  // 100ms timeout
        pollCount++;

        // Periodic debug output every 50 polls (~5 seconds)
        if (pollCount % 50 == 0) {
            fprintf(stderr, "[UDP] Poll stats: count=%llu timeouts=%llu errors=%llu packets=%llu frames=%llu\n",
                    (unsigned long long)pollCount,
                    (unsigned long long)pollTimeouts,
                    (unsigned long long)pollErrors,
                    (unsigned long long)packetsReceived_.load(),
                    (unsigned long long)framesReceived_.load());
            fflush(stderr);
        }

        if (ret < 0) {
            pollErrors++;
            continue;
        }
        if (ret == 0) {
            pollTimeouts++;
            continue;  // Timeout
        }

        // Receive packet
        sockaddr_in fromAddr;
        socklen_t fromLen = sizeof(fromAddr);

        int n = recvfrom(socket_,
                         reinterpret_cast<char*>(buffer.data()),
                         static_cast<int>(buffer.size()),
                         0,
                         reinterpret_cast<struct sockaddr*>(&fromAddr),
                         &fromLen);

        if (n <= 0) {
            continue;
        }

        // Decode CBOR packet (strict=false to ignore any trailing bytes)
        json packet;
        try {
            packet = json::from_cbor(buffer.begin(), buffer.begin() + n, /*strict=*/false);
        } catch (const json::exception& e) {
            fprintf(stderr, "[UDP] CBOR decode error: %s\n", e.what());
            fflush(stderr);
            continue;
        }

        // Validate packet structure
        if (!packet.is_array() || packet.size() < 4) {
            fprintf(stderr, "[UDP] Invalid packet structure: is_array=%d size=%zu type=%s\n",
                    packet.is_array() ? 1 : 0,
                    packet.is_array() ? packet.size() : 0,
                    packet.type_name());
            // Dump first few bytes as hex for debugging
            fprintf(stderr, "[UDP] First 32 bytes: ");
            for (int i = 0; i < std::min(n, 32); i++) {
                fprintf(stderr, "%02X ", buffer[i]);
            }
            fprintf(stderr, "\n");
            fflush(stderr);
            continue;
        }

        // Check magic and version
        if (!packet[UdpProtocol::IDX_MAGIC].is_string() ||
            packet[UdpProtocol::IDX_MAGIC].get<std::string>() != UdpProtocol::MAGIC) {
            fprintf(stderr, "[UDP] Invalid magic\n");
            fflush(stderr);
            continue;
        }
        if (!packet[UdpProtocol::IDX_VERSION].is_number_integer() ||
            packet[UdpProtocol::IDX_VERSION].get<int>() != UdpProtocol::VERSION) {
            fprintf(stderr, "[UDP] Invalid version\n");
            fflush(stderr);
            continue;
        }
        if (!packet[UdpProtocol::IDX_TYPE].is_number_integer() ||
            packet[UdpProtocol::IDX_TYPE].get<int>() != UdpProtocol::TYPE_IQ_DATA) {
            continue;  // Not I/Q data
        }

        const auto& frames = packet[UdpProtocol::IDX_FRAMES];
        if (!frames.is_array()) {
            continue;
        }

        packetsReceived_.fetch_add(1, std::memory_order_relaxed);

        // Process frames
        for (const auto& frameData : frames) {
            if (!frameData.is_array() || frameData.size() < 8) {
                continue;
            }

            IQFrame frame;
            frame.sequence = frameData[UdpProtocol::FRAME_IDX_SEQ].get<uint32_t>();
            frame.timestamp_ns = frameData[UdpProtocol::FRAME_IDX_TS].get<uint64_t>();
            frame.qsd[0].i = frameData[UdpProtocol::FRAME_IDX_I0].get<int32_t>();
            frame.qsd[0].q = frameData[UdpProtocol::FRAME_IDX_Q0].get<int32_t>();
            frame.qsd[1].i = frameData[UdpProtocol::FRAME_IDX_I1].get<int32_t>();
            frame.qsd[1].q = frameData[UdpProtocol::FRAME_IDX_Q1].get<int32_t>();
            frame.qsd[2].i = frameData[UdpProtocol::FRAME_IDX_I2].get<int32_t>();
            frame.qsd[2].q = frameData[UdpProtocol::FRAME_IDX_Q2].get<int32_t>();
            frame.flags = 0;

            // Check for dropped frames via sequence number
            if (firstFrame_.load(std::memory_order_relaxed)) {
                firstFrame_.store(false, std::memory_order_relaxed);
                lastSequence_.store(frame.sequence, std::memory_order_relaxed);
            } else {
                uint32_t expected = lastSequence_.load(std::memory_order_relaxed) + 1;
                if (frame.sequence != expected) {
                    uint32_t dropped = frame.sequence - expected;
                    framesDropped_.fetch_add(dropped, std::memory_order_relaxed);
                }
                lastSequence_.store(frame.sequence, std::memory_order_relaxed);
            }

            // Add to ring buffer
            size_t writePos = writePos_.load(std::memory_order_relaxed);
            size_t nextWritePos = (writePos + 1) % config_.receiveBufferSize;
            size_t readPos = readPos_.load(std::memory_order_acquire);

            if (nextWritePos == readPos) {
                // Buffer full - overrun
                bufferOverruns_.fetch_add(1, std::memory_order_relaxed);
                continue;
            }

            receiveBuffer_[writePos] = frame;
            writePos_.store(nextWritePos, std::memory_order_release);
            framesReceived_.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

} // namespace nexrx
