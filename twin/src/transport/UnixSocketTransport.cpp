// NexRx Digital Twin - Unix Socket Transport Implementation
//
// Copyright 2026 NexRx Project - MIT License

#include "UnixSocketTransport.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include <cstring>
#include <algorithm>

namespace nexrx {

UnixSocketTransport::UnixSocketTransport(const UnixSocketConfig& config)
    : config_(config)
{
}

UnixSocketTransport::~UnixSocketTransport() {
    disconnect();
}

bool UnixSocketTransport::connect() {
    if (isConnected()) {
        return true;
    }

    if (config_.server) {
        // Server mode: create listening socket
        listen_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
        if (listen_fd_ < 0) {
            return false;
        }

        // Remove existing socket file
        unlink(config_.path.c_str());

        struct sockaddr_un addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, config_.path.c_str(), sizeof(addr.sun_path) - 1);

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
        conn_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
        if (conn_fd_ < 0) {
            return false;
        }

        struct sockaddr_un addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, config_.path.c_str(), sizeof(addr.sun_path) - 1);

        if (::connect(conn_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
            cleanup();
            return false;
        }

        return true;
    }
}

void UnixSocketTransport::disconnect() {
    cleanup();
}

void UnixSocketTransport::cleanup() {
    if (conn_fd_ >= 0) {
        close(conn_fd_);
        conn_fd_ = -1;
    }

    if (listen_fd_ >= 0) {
        close(listen_fd_);
        listen_fd_ = -1;
    }

    if (config_.server) {
        unlink(config_.path.c_str());
    }
}

bool UnixSocketTransport::isConnected() const {
    if (config_.server) {
        return listen_fd_ >= 0;
    } else {
        return conn_fd_ >= 0;
    }
}

std::string UnixSocketTransport::name() const {
    return "UnixSocket:" + config_.path;
}

bool UnixSocketTransport::acceptClient(std::chrono::milliseconds timeout) {
    if (!config_.server || listen_fd_ < 0) {
        return false;
    }

    // Close existing client connection if any
    if (conn_fd_ >= 0) {
        close(conn_fd_);
        conn_fd_ = -1;
    }

    // Poll for incoming connection
    struct pollfd pfd;
    pfd.fd = listen_fd_;
    pfd.events = POLLIN;

    int ret = poll(&pfd, 1, static_cast<int>(timeout.count()));
    if (ret <= 0) {
        return false;
    }

    conn_fd_ = accept(listen_fd_, nullptr, nullptr);
    return conn_fd_ >= 0;
}

bool UnixSocketTransport::hasClient() const {
    return config_.server && conn_fd_ >= 0;
}

bool UnixSocketTransport::setSocketTimeout(int fd, std::chrono::milliseconds timeout) {
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

bool UnixSocketTransport::sendMessage(int fd, std::span<const uint8_t> data) {
    if (fd < 0 || data.size() > config_.maxMessageSize) {
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
        ssize_t n = send(fd, header + sent, 4 - sent, MSG_NOSIGNAL);
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

std::optional<std::vector<uint8_t>> UnixSocketTransport::receiveMessage(
    int fd,
    std::chrono::milliseconds timeout
) {
    if (fd < 0) {
        return std::nullopt;
    }

    setSocketTimeout(fd, timeout);

    // Receive length prefix
    uint8_t header[4];
    size_t received = 0;
    while (received < 4) {
        ssize_t n = recv(fd, header + received, 4 - received, 0);
        if (n <= 0) {
            return std::nullopt;
        }
        received += n;
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
        ssize_t n = recv(fd, data.data() + received, len - received, 0);
        if (n <= 0) {
            return std::nullopt;
        }
        received += n;
    }

    return data;
}

Result<std::vector<uint8_t>> UnixSocketTransport::sendRequest(
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

Result<std::vector<uint8_t>> UnixSocketTransport::receiveRequest(
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
        // Client may have disconnected
        close(conn_fd_);
        conn_fd_ = -1;
        return {{}, TransportError::Closed};
    }

    return {std::move(*request), TransportError::None};
}

TransportError UnixSocketTransport::sendResponse(std::span<const uint8_t> response) {
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
