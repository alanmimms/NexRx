#include "UDPStreamClient.hpp"
#include <cstring>
#include <iostream>

namespace nexrx {

using namespace nexrx::net;

UDPStreamClient::UDPStreamClient(const UDPStreamClientConfig& cfg)
  : config(cfg) {
  receiveBuffer.resize(cfg.receiveBufferSize);
}

UDPStreamClient::~UDPStreamClient() {
  disconnect();
}

bool UDPStreamClient::connect() {
  socket = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (socket == SOCKET_INVALID) {
    return false;
  }
  
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(config.port);
  addr.sin_addr.s_addr = INADDR_ANY;
  
  if (::bind(socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    socket_close(socket);
    socket = SOCKET_INVALID;
    return false;
  }

  running = true;
  receiveThread = std::thread(&UDPStreamClient::receiveLoop, this);
  return true;
}

void UDPStreamClient::disconnect() {
  running = false;
  if (socket != SOCKET_INVALID) {
    socket_close(socket);
    socket = SOCKET_INVALID;
  }
  if (receiveThread.joinable()) {
    receiveThread.join();
  }
}

bool UDPStreamClient::isConnected() const {
  return socket != SOCKET_INVALID;
}

std::string UDPStreamClient::name() const {
  return "UDP Client";
}

TransportError UDPStreamClient::writeBatch(std::span<const IQFrame>) {
  return TransportError::Other;
}

TransportError UDPStreamClient::write(const IQFrame&) {
  return TransportError::Other;
}

void UDPStreamClient::receiveLoop() {
  uint8_t buffer[65536];
  bool firstLog = true;
  while (running) {
    int len = ::recv(socket, reinterpret_cast<char*>(buffer), sizeof(buffer), 0);
    if (len < static_cast<int>(sizeof(IQPacketHeader))) {
      continue;
    }

    const IQPacketHeader* header = reinterpret_cast<const IQPacketHeader*>(buffer);
    if (header->magic != IQPacketHeader::MAGIC || header->version != 2) {
      continue;
    }

    if (firstLog) {
      firstLog = false;
    }

    size_t framesInPacket = header->frameCount;
    const int32_t* samples = reinterpret_cast<const int32_t*>(buffer + sizeof(IQPacketHeader));

    for (size_t i = 0; i < framesInPacket; i++) {
      IQFrame frame;
      frame.sequence = header->sequence;
      frame.timestampNS = header->timestampNS;
      frame.qsd[0].i = samples[i*6 + 0];
      frame.qsd[0].q = samples[i*6 + 1];
      frame.qsd[1].i = samples[i*6 + 2];
      frame.qsd[1].q = samples[i*6 + 3];
      frame.qsd[2].i = samples[i*6 + 4];
      frame.qsd[2].q = samples[i*6 + 5];

      size_t nextWrite = (writePos.load() + 1) % config.receiveBufferSize;
      if (nextWrite == readPos.load()) {
        bufferOverrunsCount++;
      } else {
        receiveBuffer[writePos.load()] = frame;
        writePos.store(nextWrite);
        framesReceivedCount++;
      }
    }
    packetsReceivedCount++;
  }
}

Result<IQFrame> UDPStreamClient::read(std::chrono::milliseconds timeout) {
  (void)timeout;
  if (readPos.load() == writePos.load()) {
    return {{}, TransportError::Timeout};
  }
  IQFrame frame = receiveBuffer[readPos.load()];
  readPos.store((readPos.load() + 1) % config.receiveBufferSize);
  return {frame, TransportError::None};
}

Result<std::vector<IQFrame>> UDPStreamClient::readBatch(size_t maxFrames, std::chrono::milliseconds timeout) {
  (void)timeout;
  std::vector<IQFrame> batch;
  while (batch.size() < maxFrames) {
    auto res = read(std::chrono::milliseconds(0));
    if (!res.ok()) {
      break;
    }
    batch.push_back(res.value);
  }
  if (batch.empty()) {
    return {{}, TransportError::Timeout};
  }
  return {batch, TransportError::None};
}

size_t UDPStreamClient::available() const {
  size_t w = writePos.load();
  size_t r = readPos.load();
  if (w >= r) {
    return w - r;
  }
  return config.receiveBufferSize - (r - w);
}

size_t UDPStreamClient::capacity() const {
  return config.receiveBufferSize;
}

void UDPStreamClient::clear() {
  readPos.store(writePos.load());
}

} // namespace nexrx
