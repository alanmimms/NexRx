// NexRx Digital Twin - UDP Stream Transport
//
// High-bandwidth I/Q streaming using UDP packets with CBOR encoding.
// Supports sender (server/twin) and receiver (client/app) modes.
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

#include "Transport.hpp"
#include "IQFrame.hpp"

#include <netinet/in.h>

#include <atomic>
#include <mutex>
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
// UDP Stream Transport Configuration
//======================================================================
struct UdpStreamConfig {
    std::string host = "0.0.0.0";       // Server: bind address, Client: dest address
    uint16_t port = 5001;               // UDP port
    bool server = false;                 // true = sender (twin), false = receiver (app)
    size_t framesPerPacket = 32;        // Batch size for sending
    size_t receiveBufferSize = 8192;    // Ring buffer capacity (frames)
};

//======================================================================
// UDP Stream Transport
//
// Sender (server/twin side):
//   - write() accumulates frames into packet buffer
//   - Automatically sends when buffer reaches framesPerPacket
//   - flush() sends partial buffer immediately
//
// Receiver (client/app side):
//   - Background thread receives UDP packets
//   - Frames buffered into lock-free ring buffer
//   - read()/readBatch() pulls from ring buffer
//   - Sequence numbers track dropped frames
//======================================================================
class UdpStreamTransport : public StreamTransport {
public:
    explicit UdpStreamTransport(const UdpStreamConfig& config);
    ~UdpStreamTransport() override;

    // Non-copyable
    UdpStreamTransport(const UdpStreamTransport&) = delete;
    UdpStreamTransport& operator=(const UdpStreamTransport&) = delete;

    // Transport interface
    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;
    std::string name() const override;

    // StreamTransport interface - Sender side
    TransportError write(const IQFrame& frame) override;
    TransportError writeBatch(std::span<const IQFrame> frames) override;

    // StreamTransport interface - Receiver side
    Result<IQFrame> read(std::chrono::milliseconds timeout) override;
    Result<std::vector<IQFrame>> readBatch(
        size_t maxFrames,
        std::chrono::milliseconds timeout
    ) override;

    size_t available() const override;
    size_t capacity() const override;
    void clear() override;

    // Sender: force send any buffered frames
    void flush();

    // Set destination for sender (can change dynamically)
    void setDestination(const std::string& host, uint16_t port);

    // Statistics
    uint64_t packetsReceived() const { return packetsReceived_.load(std::memory_order_relaxed); }
    uint64_t packetsSent() const { return packetsSent_.load(std::memory_order_relaxed); }
    uint64_t framesReceived() const { return framesReceived_.load(std::memory_order_relaxed); }
    uint64_t framesSent() const { return framesSent_.load(std::memory_order_relaxed); }
    uint64_t framesDropped() const { return framesDropped_.load(std::memory_order_relaxed); }
    uint64_t bufferOverruns() const { return bufferOverruns_.load(std::memory_order_relaxed); }

private:
    void receiveLoop();
    bool sendPacket();

    UdpStreamConfig config_;
    int socket_fd_ = -1;

    // Destination address (sender mode)
    sockaddr_in destAddr_;
    std::mutex destMutex_;

    // Send buffer (sender mode)
    std::vector<IQFrame> sendBuffer_;
    std::mutex sendMutex_;

    // Receive ring buffer (receiver mode)
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
    std::atomic<uint64_t> packetsSent_{0};
    std::atomic<uint64_t> framesReceived_{0};
    std::atomic<uint64_t> framesSent_{0};
    std::atomic<uint64_t> framesDropped_{0};
    std::atomic<uint64_t> bufferOverruns_{0};
};

} // namespace nexrx
