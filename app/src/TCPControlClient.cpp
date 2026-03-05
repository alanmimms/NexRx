#include "TCPControlClient.hpp"
#include <cstring>
#include <iostream>

namespace nexrx {

using namespace nexrx::net;

TCPControlClient::TCPControlClient(const TCPControlClientConfig& cfg)
  : config(cfg) {
}

TCPControlClient::~TCPControlClient() {
  disconnect();
}

bool TCPControlClient::connect() {
  socket = ::socket(AF_INET, SOCK_STREAM, 0);
  if (socket == SOCKET_INVALID) {
    return false;
  }

  sockaddr_in addr{};
  if (!parseIPv4(config.host, config.port, addr)) {
    socket_close(socket);
    socket = SOCKET_INVALID;
    return false;
  }

  if (::connect(socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    socket_close(socket);
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
  if (!sendMessage(request)) {
    return {{}, TransportError::Closed};
  }
  auto res = receiveMessage(timeout);
  if (!res) {
    return {{}, TransportError::Timeout};
  }
  return {*res, TransportError::None};
}

Result<std::vector<uint8_t>> TCPControlClient::receiveRequest(std::chrono::milliseconds) {
  return {{}, TransportError::Other};
}

TransportError TCPControlClient::sendResponse(std::span<const uint8_t>) {
  return TransportError::Other;
}

bool TCPControlClient::sendMessage(std::span<const uint8_t> data) {
  uint32_t len = static_cast<uint32_t>(data.size());
  if (::send(socket, reinterpret_cast<const char*>(&len), 4, 0) != 4) {
    return false;
  }
  return ::send(socket, reinterpret_cast<const char*>(data.data()), static_cast<int>(len), 0) == static_cast<int>(len);
}

std::optional<std::vector<uint8_t>> TCPControlClient::receiveMessage(std::chrono::milliseconds timeout) {
  setSocketTimeout(socket, static_cast<int>(timeout.count()));
  uint32_t len;
  if (::recv(socket, reinterpret_cast<char*>(&len), 4, 0) != 4) {
    return std::nullopt;
  }
  std::vector<uint8_t> buf(len);
  if (::recv(socket, reinterpret_cast<char*>(buf.data()), static_cast<int>(len), 0) != static_cast<int>(len)) {
    return std::nullopt;
  }
  return buf;
}

void TCPControlClient::cleanup() {
  if (socket != SOCKET_INVALID) {
    socket_close(socket);
    socket = SOCKET_INVALID;
  }
}

} // namespace nexrx
