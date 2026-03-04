#include "tcp-control-client.hpp"
#include <cstring>
#include <iostream>

namespace nexrx {

TCPControlClient::TCPControlClient(const TCPControlClientConfig& cfg)
    : config(cfg) {}

TCPControlClient::~TCPControlClient() {
    disconnect();
}

bool TCPControlClient::connect() {
    socket = Socket::createTCP();
    if (socket == SOCKET_INVALID) return false;

    if (!Socket::connect(socket, config.host, config.port)) {
        Socket::close(socket);
        socket = SOCKET_INVALID;
        return false;
    }

    peerAddr = config.host + ":" + std::to_string(config.port);
    return true;
}

void TCPControlClient::disconnect() {
    cleanup();
}

bool TCPControlClient::isConnected() const {
    return socket != SOCKET_INVALID;
}

std::string TCPControlClient::name() const {
    return "TCP Control Client";
}

Result<std::vector<uint8_t>> TCPControlClient::sendRequest(
    std::span<const uint8_t> request,
    std::chrono::milliseconds timeout
) {
    if (!sendMessage(request)) return TransportError::Closed;
    auto res = receiveMessage(timeout);
    if (!res) return TransportError::Timeout;
    return *res;
}

Result<std::vector<uint8_t>> TCPControlClient::receiveRequest(std::chrono::milliseconds) {
    return TransportError::Other;
}

TransportError TCPControlClient::sendResponse(std::span<const uint8_t>) {
    return TransportError::Other;
}

bool TCPControlClient::sendMessage(std::span<const uint8_t> data) {
    uint32_t len = static_cast<uint32_t>(data.size());
    if (Socket::send(socket, &len, 4) != 4) return false;
    return Socket::send(socket, data.data(), len) == (int)len;
}

std::optional<std::vector<uint8_t>> TCPControlClient::receiveMessage(std::chrono::milliseconds timeout) {
    uint32_t len;
    if (Socket::receive(socket, &len, 4) != 4) return std::nullopt;
    std::vector<uint8_t> buf(len);
    if (Socket::receive(socket, buf.data(), len) != (int)len) return std::nullopt;
    return buf;
}

void TCPControlClient::cleanup() {
    if (socket != SOCKET_INVALID) {
        Socket::close(socket);
        socket = SOCKET_INVALID;
    }
}

} // namespace nexrx
