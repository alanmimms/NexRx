// NexRx Digital Twin - Shared Memory Transport Implementation
//
// Copyright 2026 NexRx Project - MIT License

#include "SharedMemTransport.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <cstring>
#include <thread>

namespace nexrx {

SharedMemTransport::SharedMemTransport(const SharedMemConfig& cfg)
  : config(cfg) {
}

SharedMemTransport::~SharedMemTransport() {
  disconnect();
}

SharedMemTransport::SharedMemTransport(SharedMemTransport&& other) noexcept
  : config(std::move(other.config))
  , shmFD(other.shmFD)
  , mapped(other.mapped)
  , mappedSize(other.mappedSize)
  , header(other.header)
  , frames(other.frames) {
  other.shmFD = -1;
  other.mapped = nullptr;
  other.mappedSize = 0;
  other.header = nullptr;
  other.frames = nullptr;
}

SharedMemTransport& SharedMemTransport::operator=(SharedMemTransport&& other) noexcept {
  if (this != &other) {
    disconnect();
    config = std::move(other.config);
    shmFD = other.shmFD;
    mapped = other.mapped;
    mappedSize = other.mappedSize;
    header = other.header;
    frames = other.frames;

    other.shmFD = -1;
    other.mapped = nullptr;
    other.mappedSize = 0;
    other.header = nullptr;
    other.frames = nullptr;
  }
  return *this;
}

bool SharedMemTransport::connect() {
  if (isConnected()) {
    return true;
  }

  // Calculate total size needed
  mappedSize = sizeof(IQRingBufferHeader) + config.capacity * sizeof(IQFrame);

  if (config.create) {
    // Producer: create and initialize shared memory
    shmFD = shm_open(config.name.c_str(), O_CREAT | O_RDWR, 0666);
    if (shmFD < 0) {
      return false;
    }

    if (ftruncate(shmFD, static_cast<off_t>(mappedSize)) < 0) {
      cleanup();
      return false;
    }

    mapped = mmap(nullptr, mappedSize, PROT_READ | PROT_WRITE, MAP_SHARED, shmFD, 0);
    if (mapped == MAP_FAILED) {
      mapped = nullptr;
      cleanup();
      return false;
    }

    // Initialize header
    header = reinterpret_cast<IQRingBufferHeader*>(mapped);
    std::memset(header, 0, sizeof(IQRingBufferHeader));
    header->magic = IQRingBufferHeader::MAGIC;
    header->version = IQRingBufferHeader::VERSION;
    header->capacity = static_cast<uint32_t>(config.capacity);
    header->frameSize = sizeof(IQFrame);

    // Frames start after header
    frames = reinterpret_cast<IQFrame*>(
      reinterpret_cast<uint8_t*>(mapped) + sizeof(IQRingBufferHeader)
    );

    // Value-initialize all frames
    for (size_t i = 0; i < config.capacity; ++i) {
      frames[i] = IQFrame{};
    }

  } else {
    // Consumer: open existing shared memory
    shmFD = shm_open(config.name.c_str(), O_RDWR, 0666);
    if (shmFD < 0) {
      return false;
    }

    // Get actual size
    struct stat st;
    if (fstat(shmFD, &st) < 0) {
      cleanup();
      return false;
    }
    mappedSize = static_cast<size_t>(st.st_size);

    mapped = mmap(nullptr, mappedSize, PROT_READ | PROT_WRITE, MAP_SHARED, shmFD, 0);
    if (mapped == MAP_FAILED) {
      mapped = nullptr;
      cleanup();
      return false;
    }

    header = reinterpret_cast<IQRingBufferHeader*>(mapped);

    // Validate header
    if (!header->isValid()) {
      cleanup();
      return false;
    }

    config.capacity = header->capacity;
    frames = reinterpret_cast<IQFrame*>(
      reinterpret_cast<uint8_t*>(mapped) + sizeof(IQRingBufferHeader)
    );
  }

  return true;
}

void SharedMemTransport::disconnect() {
  cleanup();
}

void SharedMemTransport::cleanup() {
  if (mapped != nullptr) {
    munmap(mapped, mappedSize);
    mapped = nullptr;
  }

  if (shmFD >= 0) {
    close(shmFD);
    if (config.create) {
      shm_unlink(config.name.c_str());
    }
    shmFD = -1;
  }

  header = nullptr;
  frames = nullptr;
  mappedSize = 0;
}

bool SharedMemTransport::isConnected() const {
  return mapped != nullptr && header != nullptr;
}

std::string SharedMemTransport::name() const {
  return "SharedMem:" + config.name;
}

TransportError SharedMemTransport::write(const IQFrame& frame) {
  if (!isConnected()) {
    return TransportError::NotConnected;
  }

  // Load current positions
  uint64_t wPos = std::atomic_ref(header->writePos).load(std::memory_order_relaxed);
  uint64_t rPos = std::atomic_ref(header->readPos).load(std::memory_order_acquire);

  // Check if buffer is full
  if (wPos - rPos >= config.capacity) {
    std::atomic_ref(header->overruns).fetch_add(1, std::memory_order_relaxed);
    return TransportError::BufferFull;
  }

  // Write frame
  size_t idx = static_cast<size_t>(wPos % config.capacity);
  frames[idx] = frame;

  // Update write position
  std::atomic_ref(header->writePos).store(wPos + 1, std::memory_order_release);
  std::atomic_ref(header->writeCount).fetch_add(1, std::memory_order_relaxed);

  return TransportError::None;
}

TransportError SharedMemTransport::writeBatch(std::span<const IQFrame> framesIn) {
  for (const auto& frame : framesIn) {
    auto err = write(frame);
    if (err != TransportError::None) {
      return err;
    }
  }
  return TransportError::None;
}

Result<IQFrame> SharedMemTransport::read(std::chrono::milliseconds timeout) {
  if (!isConnected()) {
    return {{}, TransportError::NotConnected};
  }

  auto deadline = std::chrono::steady_clock::now() + timeout;

  while (true) {
    // Load current positions
    uint64_t wPos = std::atomic_ref(header->writePos).load(std::memory_order_acquire);
    uint64_t rPos = std::atomic_ref(header->readPos).load(std::memory_order_relaxed);

    // Check if data available
    if (rPos < wPos) {
      // Read frame
      size_t idx = static_cast<size_t>(rPos % config.capacity);
      IQFrame frame = frames[idx];

      // Update read position
      std::atomic_ref(header->readPos).store(rPos + 1, std::memory_order_release);
      std::atomic_ref(header->readCount).fetch_add(1, std::memory_order_relaxed);

      return {frame, TransportError::None};
    }

    // Check timeout
    if (std::chrono::steady_clock::now() >= deadline) {
      std::atomic_ref(header->underruns).fetch_add(1, std::memory_order_relaxed);
      return {{}, TransportError::Timeout};
    }

    std::this_thread::sleep_for(std::chrono::microseconds(100));
  }
}

Result<std::vector<IQFrame>> SharedMemTransport::readBatch(
  size_t maxFrames,
  std::chrono::milliseconds timeout
) {
  if (!isConnected()) {
    return {{}, TransportError::NotConnected};
  }

  std::vector<IQFrame> result;
  result.reserve(maxFrames);

  auto deadline = std::chrono::steady_clock::now() + timeout;

  while (result.size() < maxFrames) {
    uint64_t wPos = std::atomic_ref(header->writePos).load(std::memory_order_acquire);
    uint64_t rPos = std::atomic_ref(header->readPos).load(std::memory_order_relaxed);

    if (rPos < wPos) {
      size_t idx = static_cast<size_t>(rPos % config.capacity);
      result.push_back(frames[idx]);

      std::atomic_ref(header->readPos).store(rPos + 1, std::memory_order_release);
      std::atomic_ref(header->readCount).fetch_add(1, std::memory_order_relaxed);
    } else {
      if (!result.empty()) {
        break;
      }

      if (std::chrono::steady_clock::now() >= deadline) {
        std::atomic_ref(header->underruns).fetch_add(1, std::memory_order_relaxed);
        return {{}, TransportError::Timeout};
      }

      std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
  }

  return {std::move(result), TransportError::None};
}

size_t SharedMemTransport::available() const {
  return computeAvailable();
}

size_t SharedMemTransport::computeAvailable() const {
  if (!isConnected()) {
    return 0;
  }

  uint64_t wPos = std::atomic_ref(header->writePos).load(std::memory_order_acquire);
  uint64_t rPos = std::atomic_ref(header->readPos).load(std::memory_order_relaxed);

  return static_cast<size_t>(wPos - rPos);
}

size_t SharedMemTransport::capacity() const {
  return config.capacity;
}

void SharedMemTransport::clear() {
  if (!isConnected()) {
    return;
  }

  uint64_t wPos = std::atomic_ref(header->writePos).load(std::memory_order_acquire);
  std::atomic_ref(header->readPos).store(wPos, std::memory_order_release);
}

uint64_t SharedMemTransport::getOverruns() const {
  if (!isConnected()) {
    return 0;
  }
  return std::atomic_ref(header->overruns).load(std::memory_order_relaxed);
}

uint64_t SharedMemTransport::getUnderruns() const {
  if (!isConnected()) {
    return 0;
  }
  return std::atomic_ref(header->underruns).load(std::memory_order_relaxed);
}

uint64_t SharedMemTransport::getWriteCount() const {
  if (!isConnected()) {
    return 0;
  }
  return std::atomic_ref(header->writeCount).load(std::memory_order_relaxed);
}

uint64_t SharedMemTransport::getReadCount() const {
  if (!isConnected()) {
    return 0;
  }
  return std::atomic_ref(header->readCount).load(std::memory_order_relaxed);
}

} // namespace nexrx
