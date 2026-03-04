#pragma once

#include "Socket.hpp"
#include "transport/Transport.hpp"
#include "transport/IQFrame.hpp"
#include "../../twin/src/transport/iq-packet-header.hpp"

#include <atomic>
#include <chrono>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace nexrx {

struct UDPStreamClientConfig {
    uint16_t port = 5001;
    size_t receiveBufferSize = 8192;
};

class UDPStreamClient : public StreamTransport {
public:
    explicit UDPStreamClient(const UDPStreamClientConfig& config);
    ~UDPStreamClient() override;

    UDPStreamClient(const UDPStreamClient&) = delete;
    UDPStreamClient& operator=(const UDPStreamClient&) = delete;

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

    uint64_t packetsReceived() const { return packetsReceivedCount.load(); }
    uint64_t framesReceived() const { return framesReceivedCount.load(); }
    uint64_t framesDropped() const { return framesDroppedCount.load(); }
    uint64_t bufferOverruns() const { return bufferOverrunsCount.load(); }

private:
    void receiveLoop();

    UDPStreamClientConfig config;
    socket_t socket = SOCKET_INVALID;

    std::vector<IQFrame> receiveBuffer;
    std::atomic<size_t> writePos{0};
    std::atomic<size_t> readPos{0};

    std::atomic<uint32_t> lastSequence{0};
    std::atomic<bool> firstFrame{true};

    std::thread receiveThread;
    std::atomic<bool> running{false};

    std::atomic<uint64_t> packetsReceivedCount{0};
    std::atomic<uint64_t> framesReceivedCount{0};
    std::atomic<uint64_t> framesDroppedCount{0};
    std::atomic<uint64_t> bufferOverrunsCount{0};
};

} // namespace nexrx
