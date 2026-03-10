#include "UDPStreamTransport.hpp"
#include "IQPacketHeader.hpp"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

namespace nexrx {

UDPStreamTransport::UDPStreamTransport(const UDPStreamConfig& cfg)
  : config(cfg) {
  if (!config.server) {
    receiveBuffer.resize(config.receiveBufferSize);
  } else {
    // Pre-allocate for the largest possible batch
    preallocatedPacket.resize(sizeof(IQPacketHeader) + 128 * 6 * 4);
  }
}

UDPStreamTransport::~UDPStreamTransport() {
  disconnect();
}

bool UDPStreamTransport::connect() {
  socketFD = socket(AF_INET, SOCK_DGRAM, 0);
  if (socketFD < 0) {
    return false;
  }

  if (config.server) {
    /* Twin side: destination handled by setDestination() */
  } else {
    /* App side: bind to receive port */
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config.port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(socketFD, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
      close(socketFD);
      socketFD = -1;
      return false;
    }
    running = true;
    receiveThread = std::thread(&UDPStreamTransport::receiveLoop, this);
  }
  return true;
}

void UDPStreamTransport::disconnect() {
  running = false;
  if (socketFD >= 0) {
    close(socketFD);
    socketFD = -1;
  }
  if (receiveThread.joinable()) {
    receiveThread.join();
  }
}

bool UDPStreamTransport::isConnected() const {
  return socketFD >= 0;
}

std::string UDPStreamTransport::name() const {
  return "UDP Stream";
}

TransportError UDPStreamTransport::writeBatch(std::span<const IQFrame> frames) {
  if (socketFD < 0) {
    return TransportError::Closed;
  }

  const size_t packetSize = sizeof(IQPacketHeader) + frames.size() * 6 * 4;
  if (packetSize > preallocatedPacket.size()) {
      preallocatedPacket.resize(packetSize);
  }

  uint8_t* data = preallocatedPacket.data();
  IQPacketHeader* header = reinterpret_cast<IQPacketHeader*>(data);
  
  header->magic = IQPacketHeader::MAGIC;
  header->version = 2;
  header->sequence = static_cast<uint32_t>(packetsSentCount.load());
  header->timestampNS = frames[0].timestampNS;
  header->frameCount = static_cast<uint32_t>(frames.size());
  header->overrunCount = 0;

  int32_t* samples = reinterpret_cast<int32_t*>(data + sizeof(IQPacketHeader));
  for (size_t i = 0; i < frames.size(); i++) {
    samples[i*6 + 0] = frames[i].qsd[0].i;
    samples[i*6 + 1] = frames[i].qsd[0].q;
    samples[i*6 + 2] = frames[i].qsd[1].i;
    samples[i*6 + 3] = frames[i].qsd[1].q;
    samples[i*6 + 4] = frames[i].qsd[2].i;
    samples[i*6 + 5] = frames[i].qsd[2].q;
  }

  std::lock_guard<std::mutex> lock(destMutex);
  if (sendto(socketFD, data, packetSize, 0, 
             (struct sockaddr*)&destAddr, sizeof(destAddr)) < 0) {
    return TransportError::Other;
  }

  packetsSentCount++;
  framesSentCount += frames.size();
  return TransportError::None;
}

TransportError UDPStreamTransport::write(const IQFrame& frame) {
  return writeBatch(std::span<const IQFrame>(&frame, 1));
}

void UDPStreamTransport::receiveLoop() {
  uint8_t buffer[65536];
  while (running) {
    ssize_t len = recv(socketFD, buffer, sizeof(buffer), 0);
    if (len < (ssize_t)sizeof(IQPacketHeader)) {
      continue;
    }

    const IQPacketHeader* header = reinterpret_cast<const IQPacketHeader*>(buffer);
    if (header->magic != IQPacketHeader::MAGIC || header->version != 2) {
      continue;
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

Result<IQFrame> UDPStreamTransport::read(std::chrono::milliseconds timeout) {
  (void)timeout; // TODO: Implement timeout
  if (readPos.load() == writePos.load()) {
    return {{}, TransportError::Timeout};
  }
  IQFrame frame = receiveBuffer[readPos.load()];
  readPos.store((readPos.load() + 1) % config.receiveBufferSize);
  return {frame, TransportError::None};
}

Result<std::vector<IQFrame>> UDPStreamTransport::readBatch(size_t maxFrames, std::chrono::milliseconds timeout) {
  (void)timeout; // TODO: Implement timeout
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

size_t UDPStreamTransport::available() const {
  size_t w = writePos.load();
  size_t r = readPos.load();
  if (w >= r) {
    return w - r;
  }
  return config.receiveBufferSize - (r - w);
}

size_t UDPStreamTransport::capacity() const {
  return config.receiveBufferSize;
}

void UDPStreamTransport::clear() {
  readPos.store(writePos.load());
}

void UDPStreamTransport::setDestination(const std::string& hst, uint16_t port) {
  std::lock_guard<std::mutex> lock(destMutex);
  destAddr.sin_family = AF_INET;
  destAddr.sin_port = htons(port);
  inet_pton(AF_INET, hst.c_str(), &destAddr.sin_addr);
}

void UDPStreamTransport::flush() {
}

} // namespace nexrx
