// NexRx Digital Twin - Unix Socket Transport
//
// RPC transport using Unix domain sockets for control messages.
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
// Unix Socket Transport Configuration
//======================================================================
struct UnixSocketConfig {
    std::string path = "/tmp/nexrx_rpc.sock";   // Socket path
    bool server = false;                         // true = server, false = client
    size_t maxMessageSize = 65536;               // Max message size in bytes
};

//======================================================================
// Unix Socket RPC Transport
//
// Message framing: [4-byte length][payload]
// Length is little-endian uint32_t
//
// Usage:
//   Server (simulation/firmware):
//     UnixSocketTransport server(UnixSocketConfig{.path="/tmp/nexrx.sock", .server=true});
//     server.connect();  // Creates socket and listens
//     auto request = server.receiveRequest();
//     server.sendResponse(response);
//
//   Client (host app):
//     UnixSocketTransport client(UnixSocketConfig{.path="/tmp/nexrx.sock", .server=false});
//     client.connect();  // Connects to server
//     auto response = client.sendRequest(request);
//======================================================================
class UnixSocketTransport : public RpcTransport {
public:
    explicit UnixSocketTransport(const UnixSocketConfig& config);
    ~UnixSocketTransport() override;

    // Non-copyable
    UnixSocketTransport(const UnixSocketTransport&) = delete;
    UnixSocketTransport& operator=(const UnixSocketTransport&) = delete;

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

    // Server: accept a new client connection (blocking)
    bool acceptClient(std::chrono::milliseconds timeout = std::chrono::milliseconds{1000});

    // Check if a client is connected (server mode)
    bool hasClient() const;

private:
    bool sendMessage(int fd, std::span<const uint8_t> data);
    std::optional<std::vector<uint8_t>> receiveMessage(int fd, std::chrono::milliseconds timeout);
    void cleanup();
    bool setSocketTimeout(int fd, std::chrono::milliseconds timeout);

    UnixSocketConfig config_;
    int listen_fd_ = -1;        // Server: listening socket
    int conn_fd_ = -1;          // Connected socket (client or accepted connection)
};

} // namespace nexrx
