#pragma once

#include "Transport.hpp"
#include <string>
#include <vector>
#include <optional>

namespace nexrx {

struct TCPControlConfig {
    std::string host = "0.0.0.0";
    uint16_t port = 5000;
    bool server = false;
};

class TCPControlTransport : public RpcTransport {
public:
    explicit TCPControlTransport(const TCPControlConfig& config);
    ~TCPControlTransport() override;

    TCPControlTransport(const TCPControlTransport&) = delete;
    TCPControlTransport& operator=(const TCPControlTransport&) = delete;

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

    bool acceptClient(std::chrono::milliseconds timeout = std::chrono::milliseconds{1000});
    bool hasClient() const;
    std::string peerAddress() const;
    std::string peerIP() const;

private:
    bool sendMessage(int fd, std::span<const uint8_t> data);
    std::optional<std::vector<uint8_t>> receiveMessage(int fd, std::chrono::milliseconds timeout);
    void cleanup();
    bool setSocketTimeout(int fd, std::chrono::milliseconds timeout);

    TCPControlConfig config;
    int listenFd = -1;
    int connFd = -1;
    std::string peerAddr;
};

} // namespace nexrx
