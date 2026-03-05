#include "TCPControlTransport.hpp"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cstring>
#include <iostream>

namespace nexrx {

TCPControlTransport::TCPControlTransport(const TCPControlConfig& cfg)
  : config(cfg) {
}

TCPControlTransport::~TCPControlTransport() {
  cleanup();
}

bool TCPControlTransport::connect() {
  if (config.server) {
    listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd < 0) {
      return false;
    }

    int opt = 1;
    setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config.port);
    addr.sin_addr.s_addr = INADDR_ANY; // Listen on all interfaces

    if (bind(listenFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
      perror("bind");
      return false;
    }
    if (listen(listenFd, 5) < 0) {
      perror("listen");
      return false;
    }
  } else {
    connFd = socket(AF_INET, SOCK_STREAM, 0);
    if (connFd < 0) {
      return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config.port);
    inet_pton(AF_INET, config.host.c_str(), &addr.sin_addr);

    if (::connect(connFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
      return false;
    }
  }
  return true;
}

void TCPControlTransport::disconnect() {
  cleanup();
}

void TCPControlTransport::closeConnection() {
  if (connFd >= 0) {
    close(connFd);
    connFd = -1;
  }
}

bool TCPControlTransport::isConnected() const {
  return connFd >= 0;
}

std::string TCPControlTransport::name() const {
  return "TCP Control";
}

bool TCPControlTransport::acceptClient(std::chrono::milliseconds timeout) {
  if (!config.server || listenFd < 0) {
    return false;
  }

  // Close existing connection if any
  closeConnection();

  struct pollfd pfd = { listenFd, POLLIN, 0 };
  if (poll(&pfd, 1, static_cast<int>(timeout.count())) <= 0) {
    return false;
  }

  sockaddr_in clientAddr{};
  socklen_t clientLen = sizeof(clientAddr);
  connFd = accept(listenFd, (struct sockaddr*)&clientAddr, &clientLen);
  
  if (connFd >= 0) {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, ip, sizeof(ip));
    peerAddr = std::string(ip) + ":" + std::to_string(ntohs(clientAddr.sin_port));
    
    // Set keep-alive or timeout to detect half-open connections
    setSocketTimeout(connFd, std::chrono::milliseconds(5000));
    return true;
  }
  return false;
}

Result<std::vector<uint8_t>> TCPControlTransport::sendRequest(
  std::span<const uint8_t> request,
  std::chrono::milliseconds timeout
) {
  if (!sendMessage(connFd, request)) {
    closeConnection();
    return {{}, TransportError::Closed};
  }
  auto res = receiveMessage(connFd, timeout);
  if (!res) {
    // Check if it was a timeout or closure
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return {{}, TransportError::Timeout};
    }
    closeConnection();
    return {{}, TransportError::Closed};
  }
  return {*res, TransportError::None};
}

Result<std::vector<uint8_t>> TCPControlTransport::receiveRequest(std::chrono::milliseconds timeout) {
  auto res = receiveMessage(connFd, timeout);
  if (!res) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return {{}, TransportError::Timeout};
    }
    closeConnection();
    return {{}, TransportError::Closed};
  }
  return {*res, TransportError::None};
}

TransportError TCPControlTransport::sendResponse(std::span<const uint8_t> response) {
  if (!sendMessage(connFd, response)) {
    closeConnection();
    return TransportError::Closed;
  }
  return TransportError::None;
}

bool TCPControlTransport::sendMessage(int fd, std::span<const uint8_t> data) {
  if (fd < 0) return false;
  uint32_t len = static_cast<uint32_t>(data.size());
  if (send(fd, &len, 4, MSG_NOSIGNAL) != 4) {
    return false;
  }
  return send(fd, data.data(), static_cast<int>(len), MSG_NOSIGNAL) == static_cast<int>(len);
}

std::optional<std::vector<uint8_t>> TCPControlTransport::receiveMessage(int fd, std::chrono::milliseconds timeout) {
  if (fd < 0) {
    return std::nullopt;
  }
  
  setSocketTimeout(fd, timeout);
  
  uint32_t len;
  int r = recv(fd, &len, 4, 0);
  if (r <= 0) {
    return std::nullopt; // Closed or Error
  }
  if (r != 4) {
    return std::nullopt;
  }
  
  std::vector<uint8_t> buf(len);
  r = recv(fd, buf.data(), static_cast<int>(len), MSG_WAITALL);
  if (r != static_cast<int>(len)) {
    return std::nullopt;
  }
  return buf;
}

void TCPControlTransport::cleanup() {
  closeConnection();
  if (listenFd >= 0) {
    close(listenFd);
    listenFd = -1;
  }
}

bool TCPControlTransport::hasClient() const {
  return connFd >= 0;
}

std::string TCPControlTransport::peerAddress() const {
  return peerAddr;
}

std::string TCPControlTransport::peerIP() const {
  size_t pos = peerAddr.find(':');
  return (pos != std::string::npos) ? peerAddr.substr(0, pos) : peerAddr;
}

bool TCPControlTransport::setSocketTimeout(int fd, std::chrono::milliseconds timeout) {
  struct timeval tv;
  tv.tv_sec = static_cast<long>(timeout.count() / 1000);
  tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);
  return setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0 &&
         setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == 0;
}

} // namespace nexrx
