// NexRx App - TCP Control Client (Cross-platform)
//
// Client-side TCP transport for control messages to the twin.
// Uses cross-platform socket abstraction.
//
// Copyright 2026 NexRx Project - MIT License

#pragma once

#include "Socket.hpp"
#include "transport/Transport.hpp"

#include <chrono>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace nexrx {

//======================================================================
// TCP Control Client Configuration
//======================================================================
struct TcpControlClientConfig {
    std::string host = "127.0.0.1";     // Server address
    uint16_t port = 5000;                // TCP port
    size_t maxMessageSize = 65536;       // Max message size in bytes
};

//======================================================================
// TCP Control Client
//
// Client-only version of TCP transport for sending control commands.
// Message framing: [4-byte length (LE)][payload]
//======================================================================
class TcpControlClient : public RpcTransport {
public:
    explicit TcpControlClient(const TcpControlClientConfig& config);
    ~TcpControlClient() override;

    // Non-copyable
    TcpControlClient(const TcpControlClient&) = delete;
    TcpControlClient& operator=(const TcpControlClient&) = delete;

    // Transport interface
    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;
    std::string name() const override;

    // RpcTransport interface
    Result<std::vector<uint8_t>> sendRequest(
        std::span<const uint8_t> request,
        std::chrono::milliseconds timeout
    ) override;

    // Not used on client side (server-only methods)
    Result<std::vector<uint8_t>> receiveRequest(
        std::chrono::milliseconds timeout
    ) override;
    TransportError sendResponse(std::span<const uint8_t> response) override;

    // Get peer address (for logging)
    std::string peerAddress() const { return peerAddr_; }

private:
    bool sendMessage(std::span<const uint8_t> data);
    std::optional<std::vector<uint8_t>> receiveMessage(std::chrono::milliseconds timeout);
    void cleanup();

    TcpControlClientConfig config_;
    socket_t socket_ = SOCKET_INVALID;
    std::string peerAddr_;
};

} // namespace nexrx
