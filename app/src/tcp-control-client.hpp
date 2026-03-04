#pragma once

#include "Socket.hpp"
#include "transport/Transport.hpp"

#include <chrono>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace nexrx {

struct TCPControlClientConfig {
    std::string host = "127.0.0.1";
    uint16_t port = 5000;
};

class TCPControlClient : public RpcTransport {
public:
    explicit TCPControlClient(const TCPControlClientConfig& config);
    ~TCPControlClient() override;

    TCPControlClient(const TCPControlClient&) = delete;
    TCPControlClient& operator=(const TCPControlClient&) = delete;

    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;
    std::string name() const override;

    Result<std::vector<uint8_t>> sendRequest(
        std::span<const uint8_t> request,
        std::chrono::milliseconds timeout
    ) override;

    Result<std::vector<uint8_t>> receiveRequest(
        std::chrono::milliseconds timeout
    ) override;
    TransportError sendResponse(std::span<const uint8_t> response) override;

    std::string peerAddress() const { return peerAddr; }

private:
    bool sendMessage(std::span<const uint8_t> data);
    std::optional<std::vector<uint8_t>> receiveMessage(std::chrono::milliseconds timeout);
    void cleanup();

    TCPControlClientConfig config;
    socket_t socket = SOCKET_INVALID;
    std::string peerAddr;
};

} // namespace nexrx
