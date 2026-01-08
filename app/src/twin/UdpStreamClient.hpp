// NexRx App - UDP Stream Client (Cross-platform)
//
// Client-side UDP transport for receiving I/Q frames from the twin.
// Uses cross-platform socket abstraction.
//
// Copyright 2026 NexRx Project - MIT License

#pragma once

#include "net/Socket.hpp"
#include "transport/Transport.hpp"
#include "transport/IQFrame.hpp"

#include <atomic>
#include <chrono>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace nexrx {

//======================================================================
// UDP Packet Header (must match twin's UdpPacketHeader)
//======================================================================
struct UdpPacketHeader {
    uint32_t magic;         // 0x4E585251 ("NXRQ")
    uint8_t version;        // Protocol version (1)
    uint8_t flags;          // Reserved (0 = I/Q data)
    uint16_t frame_count;   // Number of IQFrames in this packet

    static constexpr uint32_t MAGIC = 0x4E585251;
    static constexpr uint8_t VERSION = 1;
    static constexpr uint8_t FLAG_IQ_DATA = 0;

    bool isValid() const {
        return magic == MAGIC && version == VERSION;
    }
};

static_assert(sizeof(UdpPacketHeader) == 8, "UdpPacketHeader must be 8 bytes");

//======================================================================
// UDP Stream Client Configuration
//======================================================================
struct UdpStreamClientConfig {
    uint16_t port = 5001;               // UDP port to listen on
    size_t receiveBufferSize = 8192;    // Ring buffer capacity (frames)
};

//======================================================================
// UDP Stream Client
//
// Client-only version of UDP transport for receiving I/Q frames.
// Runs a background thread to receive packets and buffer them.
//======================================================================
class UdpStreamClient : public StreamTransport {
public:
    explicit UdpStreamClient(const UdpStreamClientConfig& config);
    ~UdpStreamClient() override;

    // Non-copyable
    UdpStreamClient(const UdpStreamClient&) = delete;
    UdpStreamClient& operator=(const UdpStreamClient&) = delete;

    // Transport interface
    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;
    std::string name() const override;

    // StreamTransport interface - Writing not supported (client is receiver only)
    TransportError write(const IQFrame& frame) override;
    TransportError writeBatch(std::span<const IQFrame> frames) override;

    // StreamTransport interface - Reading
    Result<IQFrame> read(std::chrono::milliseconds timeout) override;
    Result<std::vector<IQFrame>> readBatch(
        size_t maxFrames,
        std::chrono::milliseconds timeout
    ) override;

    size_t available() const override;
    size_t capacity() const override;
    void clear() override;

    // Statistics
    uint64_t packetsReceived() const { return packetsReceived_.load(std::memory_order_relaxed); }
    uint64_t framesReceived() const { return framesReceived_.load(std::memory_order_relaxed); }
    uint64_t framesDropped() const { return framesDropped_.load(std::memory_order_relaxed); }
    uint64_t bufferOverruns() const { return bufferOverruns_.load(std::memory_order_relaxed); }

private:
    void receiveLoop();

    UdpStreamClientConfig config_;
    socket_t socket_ = SOCKET_INVALID;

    // Receive ring buffer
    std::vector<IQFrame> receiveBuffer_;
    std::atomic<size_t> writePos_{0};
    std::atomic<size_t> readPos_{0};

    // Sequence tracking for drop detection
    std::atomic<uint32_t> lastSequence_{0};
    std::atomic<bool> firstFrame_{true};

    // Background receive thread
    std::thread receiveThread_;
    std::atomic<bool> running_{false};

    // Statistics
    std::atomic<uint64_t> packetsReceived_{0};
    std::atomic<uint64_t> framesReceived_{0};
    std::atomic<uint64_t> framesDropped_{0};
    std::atomic<uint64_t> bufferOverruns_{0};
};

} // namespace nexrx
