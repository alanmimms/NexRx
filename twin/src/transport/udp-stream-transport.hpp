/**
 * udp-stream-transport.hpp
 * High-bandwidth IQ streaming using UDP packets.
 */

#pragma once

#include "Transport.hpp"
#include "IQFrame.hpp"

#include <netinet/in.h>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <span>

namespace nexrx {

struct UDPStreamConfig {
    std::string host = "0.0.0.0";
    uint16_t port = 5001;
    bool server = false;
    size_t framesPerPacket = 32;
    size_t receiveBufferSize = 8192;
};

class UDPStreamTransport : public StreamTransport {
public:
    explicit UDPStreamTransport(const UDPStreamConfig& config);
    ~UDPStreamTransport() override;

    UDPStreamTransport(const UDPStreamTransport&) = delete;
    UDPStreamTransport& operator=(const UDPStreamTransport&) = delete;

    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;
    std::string name() const override;

    TransportError write(const IQFrame& frame) override;
    TransportError writeBatch(std::span<const IQFrame> frames) override;

    Result<IQFrame> read(std::chrono::milliseconds timeout) override;
    Result<std::vector<IQFrame>> readBatch(
        size_t maxFrames,
        std::chrono::milliseconds timeout
    ) override;

    size_t available() const override;
    size_t capacity() const override;
    void clear() override;

    void flush();
    void setDestination(const std::string& host, uint16_t port);

    uint64_t packetsReceived() const { return packetsReceivedCount.load(); }
    uint64_t packetsSent() const { return packetsSentCount.load(); }
    uint64_t framesReceived() const { return framesReceivedCount.load(); }
    uint64_t framesSent() const { return framesSentCount.load(); }
    uint64_t framesDropped() const { return framesDroppedCount.load(); }
    uint64_t bufferOverruns() const { return bufferOverrunsCount.load(); }

private:
    void receiveLoop();
    bool sendPacket();

    UDPStreamConfig config;
    int socketFd = -1;

    sockaddr_in destAddr;
    std::mutex destMutex;

    std::vector<IQFrame> sendBuffer;
    std::mutex sendMutex;

    std::vector<IQFrame> receiveBuffer;
    std::atomic<size_t> writePos{0};
    std::atomic<size_t> readPos{0};

    std::atomic<uint32_t> lastSequence{0};
    std::atomic<bool> firstFrame{true};

    std::thread receiveThread;
    std::atomic<bool> running{false};

    std::atomic<uint64_t> packetsReceivedCount{0};
    std::atomic<uint64_t> packetsSentCount{0};
    std::atomic<uint64_t> framesReceivedCount{0};
    std::atomic<uint64_t> framesSentCount{0};
    std::atomic<uint64_t> framesDroppedCount{0};
    std::atomic<uint64_t> bufferOverrunsCount{0};
};

} // namespace nexrx
