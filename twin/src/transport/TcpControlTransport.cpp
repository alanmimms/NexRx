// NexRx Digital Twin - TCP Control Transport Implementation
//
// Copyright 2026 NexRx Project - MIT License

#include "TcpControlTransport.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include <cbor.h>
#include <cstring>
#include <algorithm>

namespace nexrx {

TcpControlTransport::TcpControlTransport(const TcpControlConfig& config)
    : config_(config)
{
}

TcpControlTransport::~TcpControlTransport() {
    disconnect();
}

bool TcpControlTransport::connect() {
    if (isConnected()) {
        return true;
    }

    if (config_.server) {
        // Server mode: create listening socket
        listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd_ < 0) {
            return false;
        }

        // Allow address reuse
        int opt = 1;
        setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(config_.port);

        if (config_.host == "0.0.0.0" || config_.host.empty()) {
            addr.sin_addr.s_addr = INADDR_ANY;
        } else {
            if (inet_pton(AF_INET, config_.host.c_str(), &addr.sin_addr) <= 0) {
                cleanup();
                return false;
            }
        }

        if (bind(listen_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
            cleanup();
            return false;
        }

        if (listen(listen_fd_, 1) < 0) {
            cleanup();
            return false;
        }

        return true;

    } else {
        // Client mode: connect to server
        conn_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (conn_fd_ < 0) {
            return false;
        }

        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(config_.port);

        if (inet_pton(AF_INET, config_.host.c_str(), &addr.sin_addr) <= 0) {
            cleanup();
            return false;
        }

        if (::connect(conn_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
            cleanup();
            return false;
        }

        // Disable Nagle's algorithm for lower latency
        int flag = 1;
        setsockopt(conn_fd_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

        peerAddr_ = config_.host + ":" + std::to_string(config_.port);
        return true;
    }
}

void TcpControlTransport::disconnect() {
    cleanup();
}

void TcpControlTransport::cleanup() {
    if (conn_fd_ >= 0) {
        close(conn_fd_);
        conn_fd_ = -1;
    }

    if (listen_fd_ >= 0) {
        close(listen_fd_);
        listen_fd_ = -1;
    }

    peerAddr_.clear();
}

bool TcpControlTransport::isConnected() const {
    if (config_.server) {
        return listen_fd_ >= 0;
    } else {
        return conn_fd_ >= 0;
    }
}

std::string TcpControlTransport::name() const {
    if (config_.server) {
        return "TcpControl:server:" + std::to_string(config_.port);
    } else {
        return "TcpControl:" + config_.host + ":" + std::to_string(config_.port);
    }
}

std::string TcpControlTransport::peerAddress() const {
    return peerAddr_;
}

std::string TcpControlTransport::peerIP() const {
    // peerAddr_ is "ip:port", extract just the IP
    auto pos = peerAddr_.find(':');
    if (pos != std::string::npos) {
        return peerAddr_.substr(0, pos);
    }
    return peerAddr_;
}

bool TcpControlTransport::acceptClient(std::chrono::milliseconds timeout) {
    if (!config_.server || listen_fd_ < 0) {
        return false;
    }

    // Close existing client connection if any
    if (conn_fd_ >= 0) {
        close(conn_fd_);
        conn_fd_ = -1;
        peerAddr_.clear();
    }

    // Poll for incoming connection
    // timeout of 0 means wait forever (use -1 for poll)
    struct pollfd pfd;
    pfd.fd = listen_fd_;
    pfd.events = POLLIN;

    int poll_timeout = (timeout.count() == 0) ? -1 : static_cast<int>(timeout.count());
    int ret = poll(&pfd, 1, poll_timeout);
    if (ret <= 0) {
        return false;
    }

    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);
    conn_fd_ = accept(listen_fd_, reinterpret_cast<struct sockaddr*>(&clientAddr), &addrLen);

    if (conn_fd_ >= 0) {
        // Disable Nagle's algorithm for lower latency
        int flag = 1;
        setsockopt(conn_fd_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

        // Store peer address for logging
        char addrStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, addrStr, sizeof(addrStr));
        peerAddr_ = std::string(addrStr) + ":" + std::to_string(ntohs(clientAddr.sin_port));
        return true;
    }

    return false;
}

bool TcpControlTransport::hasClient() const {
    return config_.server && conn_fd_ >= 0;
}

bool TcpControlTransport::setSocketTimeout(int fd, std::chrono::milliseconds timeout) {
    struct timeval tv;
    tv.tv_sec = timeout.count() / 1000;
    tv.tv_usec = (timeout.count() % 1000) * 1000;

    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        return false;
    }
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        return false;
    }
    return true;
}

bool TcpControlTransport::sendMessage(int fd, std::span<const uint8_t> data) {
    if (fd < 0) {
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
        ssize_t n = send(fd, header + sent, headerLen - sent, MSG_NOSIGNAL);
        if (n <= 0) {
            return false;
        }
        sent += n;
    }

    // Send payload
    sent = 0;
    while (sent < data.size()) {
        ssize_t n = send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        if (n <= 0) {
            return false;
        }
        sent += n;
    }

    return true;
}

std::optional<std::vector<uint8_t>> TcpControlTransport::receiveMessage(
    int fd,
    std::chrono::milliseconds timeout
) {
    if (fd < 0) {
        return std::nullopt;
    }

    setSocketTimeout(fd, timeout);

    // Receive first byte of CBOR length prefix
    uint8_t firstByte;
    ssize_t n = recv(fd, &firstByte, 1, 0);
    if (n < 0) return std::nullopt;
    if (n == 0) { errno = ECONNRESET; return std::nullopt; }

    uint64_t len = 0;
    if (firstByte < 0x18) {
        len = firstByte;
    } else if (firstByte <= 0x1b) {
        size_t extraBytes = 1 << (firstByte - 0x18);
        uint8_t buffer[8];
        size_t received = 0;
        while (received < extraBytes) {
            n = recv(fd, buffer + received, extraBytes - received, 0);
            if (n <= 0) { errno = ECONNRESET; return std::nullopt; }
            received += n;
        }
        if (extraBytes == 1) len = buffer[0];
        else if (extraBytes == 2) len = (static_cast<uint64_t>(buffer[0]) << 8) | buffer[1];
        else if (extraBytes == 4) len = (static_cast<uint64_t>(buffer[0]) << 24) | (static_cast<uint64_t>(buffer[1]) << 16) | (static_cast<uint64_t>(buffer[2]) << 8) | buffer[3];
        else if (extraBytes == 8) {
            for (int i = 0; i < 8; ++i) len = (len << 8) | buffer[i];
        }
    } else {
        // Not an unsigned integer (or indefinite length, which we don't support for framing)
        return std::nullopt;
    }

    // Receive payload
    std::vector<uint8_t> data(len);
    size_t received = 0;
    while (received < len) {
        n = recv(fd, data.data() + received, len - received, 0);
        if (n < 0) return std::nullopt;
        if (n == 0) { errno = ECONNRESET; return std::nullopt; }
        received += n;
    }

    return data;
}

Result<std::vector<uint8_t>> TcpControlTransport::sendRequest(
    std::span<const uint8_t> request,
    std::chrono::milliseconds timeout
) {
    if (config_.server) {
        // Server should use receiveRequest/sendResponse
        return {{}, TransportError::InvalidData};
    }

    if (conn_fd_ < 0) {
        return {{}, TransportError::NotConnected};
    }

    if (!sendMessage(conn_fd_, request)) {
        return {{}, TransportError::IoError};
    }

    auto response = receiveMessage(conn_fd_, timeout);
    if (!response) {
        return {{}, TransportError::Timeout};
    }

    return {std::move(*response), TransportError::None};
}

Result<std::vector<uint8_t>> TcpControlTransport::receiveRequest(
    std::chrono::milliseconds timeout
) {
    if (!config_.server) {
        return {{}, TransportError::InvalidData};
    }

    // Accept client if needed
    if (conn_fd_ < 0) {
        if (!acceptClient(timeout)) {
            return {{}, TransportError::Timeout};
        }
    }

    auto request = receiveMessage(conn_fd_, timeout);
    if (!request) {
        // Check if it was a timeout (EAGAIN/EWOULDBLOCK) vs real disconnect
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Just a timeout - keep connection open
            return {{}, TransportError::Timeout};
        }
        // Client disconnected or error - close connection
        close(conn_fd_);
        conn_fd_ = -1;
        peerAddr_.clear();
        return {{}, TransportError::Closed};
    }

    return {std::move(*request), TransportError::None};
}

TransportError TcpControlTransport::sendResponse(std::span<const uint8_t> response) {
    if (!config_.server) {
        return TransportError::InvalidData;
    }

    if (conn_fd_ < 0) {
        return TransportError::NotConnected;
    }

    if (!sendMessage(conn_fd_, response)) {
        return TransportError::IoError;
    }

    return TransportError::None;
}

} // namespace nexrx
