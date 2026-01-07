// NexRx Digital Twin - TCP Control Transport
//
// RPC transport using TCP sockets for control messages.
// Supports both client (host app) and server (firmware/simulation) modes.
//
// Copyright 2026 NexRx Project - MIT License

#pragma once

#include "Transport.hpp"

#include <string>
#include <vector>
#include <optional>

namespace nexrx {

//======================================================================
// TCP Control Transport Configuration
//======================================================================
struct TcpControlConfig {
    std::string host = "0.0.0.0";           // Server: bind address, Client: connect address
    uint16_t port = 5000;                    // TCP port
    bool server = false;                     // true = server, false = client
    size_t maxMessageSize = 65536;           // Max message size in bytes
};

//======================================================================
// TCP Control Transport
//
// Message framing: [4-byte length][payload]
// Length is little-endian uint32_t
//
// Usage:
//   Server (simulation/firmware):
//     TcpControlTransport server(TcpControlConfig{.host="0.0.0.0", .port=5000, .server=true});
//     server.connect();  // Creates socket and listens
//     server.acceptClient(timeout);
//     auto request = server.receiveRequest();
//     server.sendResponse(response);
//
//   Client (host app):
//     TcpControlTransport client(TcpControlConfig{.host="127.0.0.1", .port=5000, .server=false});
//     client.connect();  // Connects to server
//     auto response = client.sendRequest(request);
//======================================================================
class TcpControlTransport : public RpcTransport {
public:
    explicit TcpControlTransport(const TcpControlConfig& config);
    ~TcpControlTransport() override;

    // Non-copyable
    TcpControlTransport(const TcpControlTransport&) = delete;
    TcpControlTransport& operator=(const TcpControlTransport&) = delete;

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

    Result<std::vector<uint8_t>> receiveRequest(
        std::chrono::milliseconds timeout
    ) override;

    TransportError sendResponse(std::span<const uint8_t> response) override;

    // Server: accept a new client connection (blocking with timeout)
    bool acceptClient(std::chrono::milliseconds timeout = std::chrono::milliseconds{1000});

    // Check if a client is connected (server mode)
    bool hasClient() const;

    // Get peer address (for logging)
    std::string peerAddress() const;

    // Get peer IP only (without port) - for UDP streaming destination
    std::string peerIP() const;

private:
    bool sendMessage(int fd, std::span<const uint8_t> data);
    std::optional<std::vector<uint8_t>> receiveMessage(int fd, std::chrono::milliseconds timeout);
    void cleanup();
    bool setSocketTimeout(int fd, std::chrono::milliseconds timeout);

    TcpControlConfig config_;
    int listen_fd_ = -1;        // Server: listening socket
    int conn_fd_ = -1;          // Connected socket (client or accepted connection)
    std::string peerAddr_;      // Connected peer address
};

} // namespace nexrx
