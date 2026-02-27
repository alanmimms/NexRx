// NexRx App - TCP Control Client Implementation
//
// Copyright 2026 NexRx Project - MIT License

#include "TcpControlClient.hpp"

#include <cbor.h>
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
        return false;
    }

    // Send length prefix as CBOR unsigned integer
    uint8_t header[9];
    CborEncoder encoder;
    cbor_encoder_init(&encoder, header, sizeof(header), 0);
    cbor_encode_uint(&encoder, data.size());
    size_t headerLen = cbor_encoder_get_buffer_size(&encoder, header);

    // Send header
    size_t sent = 0;
    while (sent < headerLen) {
        int n = send(socket_, reinterpret_cast<const char*>(header + sent),
                     static_cast<int>(headerLen - sent), MSG_NOSIGNAL);
        if (n <= 0) {
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

    // Receive first byte of CBOR length prefix
    uint8_t firstByte;
    int n = recv(socket_, reinterpret_cast<char*>(&firstByte), 1, 0);
    if (n <= 0) return std::nullopt;

    uint64_t len = 0;
    if (firstByte < 0x18) {
        len = firstByte;
    } else if (firstByte <= 0x1b) {
        size_t extraBytes = 1 << (firstByte - 0x18);
        uint8_t buffer[8];
        size_t received = 0;
        while (received < extraBytes) {
            n = recv(socket_, reinterpret_cast<char*>(buffer + received),
                     static_cast<int>(extraBytes - received), 0);
            if (n <= 0) return std::nullopt;
            received += static_cast<size_t>(n);
        }
        if (extraBytes == 1) len = buffer[0];
        else if (extraBytes == 2) len = (static_cast<uint64_t>(buffer[0]) << 8) | buffer[1];
        else if (extraBytes == 4) len = (static_cast<uint64_t>(buffer[0]) << 24) | (static_cast<uint64_t>(buffer[1]) << 16) | (static_cast<uint64_t>(buffer[2]) << 8) | buffer[3];
        else if (extraBytes == 8) {
            for (int i = 0; i < 8; ++i) len = (len << 8) | buffer[i];
        }
    } else {
        return std::nullopt;
    }

    // Receive payload
    std::vector<uint8_t> data(len);
    size_t received = 0;
    while (received < len) {
        n = recv(socket_, reinterpret_cast<char*>(data.data() + received),
                 static_cast<int>(len - received), 0);
        if (n <= 0) return std::nullopt;
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
