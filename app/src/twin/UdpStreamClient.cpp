// NexRx App - UDP Stream Client Implementation
//
// Copyright 2026 NexRx Project - MIT License

#include "UdpStreamClient.hpp"

#include <cstring>
#include <algorithm>

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

    // Create UDP socket
    socket_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_ == SOCKET_INVALID) {
        return false;
    }

    // Allow address reuse
    net::setReuseAddr(socket_, true);

    // Bind to receive port
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config_.port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(socket_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        socket_close(socket_);
        socket_ = SOCKET_INVALID;
        return false;
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

void UdpStreamClient::receiveLoop() {
    constexpr size_t MAX_FRAMES_PER_PACKET = 64;
    std::vector<uint8_t> buffer(sizeof(UdpPacketHeader) + MAX_FRAMES_PER_PACKET * sizeof(IQFrame));

    while (running_.load(std::memory_order_acquire)) {
        // Poll for data with timeout
        pollfd_t pfd;
        pfd.fd = socket_;
        pfd.events = POLLIN;
        pfd.revents = 0;

        int ret = socket_poll(&pfd, 1, 100);  // 100ms timeout
        if (ret <= 0) {
            continue;  // Timeout or error
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

        if (n < static_cast<int>(sizeof(UdpPacketHeader))) {
            continue;  // Too small or error
        }

        // Parse header
        UdpPacketHeader header;
        std::memcpy(&header, buffer.data(), sizeof(header));

        if (!header.isValid()) {
            continue;  // Invalid packet
        }

        size_t expectedSize = sizeof(UdpPacketHeader) +
                              header.frame_count * sizeof(IQFrame);
        if (static_cast<size_t>(n) < expectedSize) {
            continue;  // Truncated packet
        }

        packetsReceived_.fetch_add(1, std::memory_order_relaxed);

        // Process frames
        const IQFrame* frames = reinterpret_cast<const IQFrame*>(
            buffer.data() + sizeof(UdpPacketHeader));

        for (uint16_t i = 0; i < header.frame_count; i++) {
            const IQFrame& frame = frames[i];

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
