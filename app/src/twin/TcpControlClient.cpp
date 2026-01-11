// NexRx App - TCP Control Client Implementation
//
// Copyright 2026 NexRx Project - MIT License

#include "TcpControlClient.hpp"

#include <algorithm>
#include <cerrno>
#include <iostream>

namespace nexrx {

TcpControlClient::TcpControlClient(const TcpControlClientConfig& config)
    : config_(config)
{
}

TcpControlClient::~TcpControlClient() {
    disconnect();
}

bool TcpControlClient::connect() {
    if (isConnected()) {
        return true;
    }

    // Create TCP socket
    socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_ == SOCKET_INVALID) {
        return false;
    }

    // Parse server address
    sockaddr_in addr;
    if (!net::parseIPv4(config_.host, config_.port, addr)) {
        cleanup();
        return false;
    }

    // Connect to server
    if (::connect(socket_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        cleanup();
        return false;
    }

    // Disable Nagle's algorithm for lower latency
    net::setNoDelay(socket_, true);

    peerAddr_ = config_.host + ":" + std::to_string(config_.port);
    return true;
}

void TcpControlClient::disconnect() {
    cleanup();
}

void TcpControlClient::cleanup() {
    if (socket_ != SOCKET_INVALID) {
        socket_close(socket_);
        socket_ = SOCKET_INVALID;
    }
    peerAddr_.clear();
}

bool TcpControlClient::isConnected() const {
    return socket_ != SOCKET_INVALID;
}

std::string TcpControlClient::name() const {
    return "TcpControlClient:" + config_.host + ":" + std::to_string(config_.port);
}

bool TcpControlClient::sendMessage(std::span<const uint8_t> data) {
    if (socket_ == SOCKET_INVALID) {
        std::cerr << "[TcpClient] sendMessage: socket invalid" << std::endl;
        return false;
    }
    if (data.size() > config_.maxMessageSize) {
        std::cerr << "[TcpClient] sendMessage: data too large" << std::endl;
        return false;
    }

    // Send length prefix (4 bytes, little-endian)
    uint32_t len = static_cast<uint32_t>(data.size());
    uint8_t header[4];
    header[0] = len & 0xFF;
    header[1] = (len >> 8) & 0xFF;
    header[2] = (len >> 16) & 0xFF;
    header[3] = (len >> 24) & 0xFF;

    // Send header
    size_t sent = 0;
    while (sent < 4) {
        int n = send(socket_, reinterpret_cast<const char*>(header + sent),
                     static_cast<int>(4 - sent), MSG_NOSIGNAL);
        if (n <= 0) {
            std::cerr << "[TcpClient] sendMessage: header send failed, n=" << n
                      << " errno=" << errno << std::endl;
            return false;
        }
        sent += static_cast<size_t>(n);
    }

    // Send payload
    sent = 0;
    while (sent < data.size()) {
        int n = send(socket_, reinterpret_cast<const char*>(data.data() + sent),
                     static_cast<int>(data.size() - sent), MSG_NOSIGNAL);
        if (n <= 0) {
            std::cerr << "[TcpClient] sendMessage: payload send failed, n=" << n
                      << " errno=" << errno << std::endl;
            return false;
        }
        sent += static_cast<size_t>(n);
    }

    return true;
}

std::optional<std::vector<uint8_t>> TcpControlClient::receiveMessage(
    std::chrono::milliseconds timeout
) {
    if (socket_ == SOCKET_INVALID) {
        return std::nullopt;
    }

    net::setSocketTimeout(socket_, static_cast<int>(timeout.count()));

    // Receive length prefix
    uint8_t header[4];
    size_t received = 0;
    while (received < 4) {
        int n = recv(socket_, reinterpret_cast<char*>(header + received),
                     static_cast<int>(4 - received), 0);
        if (n <= 0) {
            return std::nullopt;
        }
        received += static_cast<size_t>(n);
    }

    uint32_t len = header[0] |
                   (static_cast<uint32_t>(header[1]) << 8) |
                   (static_cast<uint32_t>(header[2]) << 16) |
                   (static_cast<uint32_t>(header[3]) << 24);

    if (len > config_.maxMessageSize) {
        return std::nullopt;
    }

    // Receive payload
    std::vector<uint8_t> data(len);
    received = 0;
    while (received < len) {
        int n = recv(socket_, reinterpret_cast<char*>(data.data() + received),
                     static_cast<int>(len - received), 0);
        if (n <= 0) {
            return std::nullopt;
        }
        received += static_cast<size_t>(n);
    }

    return data;
}

Result<std::vector<uint8_t>> TcpControlClient::sendRequest(
    std::span<const uint8_t> request,
    std::chrono::milliseconds timeout
) {
    if (socket_ == SOCKET_INVALID) {
        return {{}, TransportError::NotConnected};
    }

    if (!sendMessage(request)) {
        return {{}, TransportError::IoError};
    }

    auto response = receiveMessage(timeout);
    if (!response) {
        return {{}, TransportError::Timeout};
    }

    return {std::move(*response), TransportError::None};
}

Result<std::vector<uint8_t>> TcpControlClient::receiveRequest(
    std::chrono::milliseconds /*timeout*/
) {
    // Client doesn't receive requests
    return {{}, TransportError::InvalidData};
}

TransportError TcpControlClient::sendResponse(std::span<const uint8_t> /*response*/) {
    // Client doesn't send responses
    return TransportError::InvalidData;
}

} // namespace nexrx
