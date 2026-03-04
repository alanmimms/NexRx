#include "tcp-control-transport.hpp"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cstring>
#include <iostream>

namespace nexrx {

TCPControlTransport::TCPControlTransport(const TCPControlConfig& cfg)
    : config(cfg) {}

TCPControlTransport::~TCPControlTransport() {
    cleanup();
}

bool TCPControlTransport::connect() {
    if (config.server) {
        listenFd = socket(AF_INET, SOCK_STREAM, 0);
        if (listenFd < 0) return false;

        int opt = 1;
        setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(config.port);
        addr.sin_addr.s_addr = inet_addr(config.host.c_str());

        if (bind(listenFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) return false;
        if (listen(listenFd, 1) < 0) return false;
    } else {
        connFd = socket(AF_INET, SOCK_STREAM, 0);
        if (connFd < 0) return false;

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(config.port);
        inet_pton(AF_INET, config.host.c_str(), &addr.sin_addr);

        if (::connect(connFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) return false;
    }
    return true;
}

void TCPControlTransport::disconnect() {
    cleanup();
}

bool TCPControlTransport::isConnected() const {
    return connFd >= 0;
}

std::string TCPControlTransport::name() const {
    return "TCP Control";
}

bool TCPControlTransport::acceptClient(std::chrono::milliseconds timeout) {
    if (!config.server || listenFd < 0) return false;

    struct pollfd pfd = { listenFd, POLLIN, 0 };
    if (poll(&pfd, 1, (int)timeout.count()) <= 0) return false;

    sockaddr_in clientAddr{};
    socklen_t clientLen = sizeof(clientAddr);
    connFd = accept(listenFd, (struct sockaddr*)&clientAddr, &clientLen);
    
    if (connFd >= 0) {
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, ip, sizeof(ip));
        peerAddr = std::string(ip) + ":" + std::to_string(ntohs(clientAddr.sin_port));
        return true;
    }
    return false;
}

Result<std::vector<uint8_t>> TCPControlTransport::sendRequest(
    std::span<const uint8_t> request,
    std::chrono::milliseconds timeout
) {
    if (!sendMessage(connFd, request)) return TransportError::Closed;
    auto res = receiveMessage(connFd, timeout);
    if (!res) return TransportError::Timeout;
    return *res;
}

Result<std::vector<uint8_t>> TCPControlTransport::receiveRequest(std::chrono::milliseconds timeout) {
    auto res = receiveMessage(connFd, timeout);
    if (!res) return TransportError::Timeout;
    return *res;
}

TransportError TCPControlTransport::sendResponse(std::span<const uint8_t> response) {
    return sendMessage(connFd, response) ? TransportError::None : TransportError::Closed;
}

bool TCPControlTransport::sendMessage(int fd, std::span<const uint8_t> data) {
    uint32_t len = static_cast<uint32_t>(data.size());
    if (send(fd, &len, 4, 0) != 4) return false;
    return send(fd, data.data(), len, 0) == (int)len;
}

std::optional<std::vector<uint8_t>> TCPControlTransport::receiveMessage(int fd, std::chrono::milliseconds timeout) {
    if (fd < 0) return std::nullopt;
    uint32_t len;
    if (recv(fd, &len, 4, 0) != 4) return std::nullopt;
    std::vector<uint8_t> buf(len);
    if (recv(fd, buf.data(), len, 0) != (int)len) return std::nullopt;
    return buf;
}

void TCPControlTransport::cleanup() {
    if (connFd >= 0) { close(connFd); connFd = -1; }
    if (listenFd >= 0) { close(listenFd); listenFd = -1; }
}

bool TCPControlTransport::hasClient() const { return connFd >= 0; }
std::string TCPControlTransport::peerAddress() const { return peerAddr; }
std::string TCPControlTransport::peerIP() const {
    size_t pos = peerAddr.find(':');
    return (pos != std::string::npos) ? peerAddr.substr(0, pos) : peerAddr;
}

bool TCPControlTransport::setSocketTimeout(int fd, std::chrono::milliseconds timeout) {
    struct timeval tv;
    tv.tv_sec = (long)(timeout.count() / 1000);
    tv.tv_usec = (long)((timeout.count() % 1000) * 1000);
    return setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0;
}

} // namespace nexrx
