// NexRx App - UDP Stream Client (Cross-platform)
//
// Client-side UDP transport for receiving I/Q frames from the twin.
// Uses CBOR encoding for cross-platform compatibility.
//
// CBOR packet format (array-based for compactness):
// [
//   "NXRQ",                              // magic string (index 0)
//   1,                                   // version (index 1)
//   0,                                   // type: 0=IQ, 1=TX audio (index 2)
//   [                                    // frames array (index 3)
//     [seq, ts_ns, i0, q0, i1, q1, i2, q2],  // frame as flat array
//     ...
//   ]
// ]
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
// UDP Protocol Constants (CBOR-encoded packets)
//======================================================================
namespace UdpProtocol {
    constexpr const char* MAGIC = "NXRQ";
    constexpr int VERSION = 1;
    constexpr int TYPE_IQ_DATA = 0;
    constexpr int TYPE_TX_AUDIO = 1;  // Future: NexRig TX audio

    // Array indices in CBOR packet
    constexpr size_t IDX_MAGIC = 0;
    constexpr size_t IDX_VERSION = 1;
    constexpr size_t IDX_TYPE = 2;
    constexpr size_t IDX_FRAMES = 3;

    // Array indices within each frame
    constexpr size_t FRAME_IDX_SEQ = 0;
    constexpr size_t FRAME_IDX_TS = 1;
    constexpr size_t FRAME_IDX_I0 = 2;
    constexpr size_t FRAME_IDX_Q0 = 3;
    constexpr size_t FRAME_IDX_I1 = 4;
    constexpr size_t FRAME_IDX_Q1 = 5;
    constexpr size_t FRAME_IDX_I2 = 6;
    constexpr size_t FRAME_IDX_Q2 = 7;
}

//======================================================================
// UDP Stream Client Configuration
//======================================================================
struct UdpStreamClientConfig {
    uint16_t port = 5001;               // UDP port to listen on
    size_t receiveBufferSize = 8192;    // Ring buffer capacity (frames)
    std::string serverHost = "";        // Server address for NAT hole punch
    uint16_t serverPort = 5001;         // Server port for NAT hole punch
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

    // Send NAT hole punch packet to server
    bool sendHolePunch();

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
