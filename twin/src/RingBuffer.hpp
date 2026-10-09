#pragma once

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <mutex>
#include <condition_variable>

namespace nexrx {

template <typename T, size_t CAPACITY = 65536>
class RingBuffer {
  static_assert((CAPACITY & (CAPACITY - 1)) == 0, "CAPACITY must be a power of two");

public:
  RingBuffer() = default;

  size_t write(const T* data, size_t count) {
    size_t w = writeIndex.load(std::memory_order_relaxed);
    size_t r = readIndex.load(std::memory_order_acquire);
    size_t availableSlots = CAPACITY - (w - r);
    size_t toWrite = std::min(count, availableSlots);

    if (toWrite == 0) {
      return 0;
    }

    size_t mask = CAPACITY - 1;
    for (int i = 0; i < static_cast<int>(toWrite); ++i) {
      buffer[(w + i) & mask] = data[i];
    }

    writeIndex.store(w + toWrite, std::memory_order_release);
    return toWrite;
  }

  size_t read(T* outData, size_t count) {
    size_t r = readIndex.load(std::memory_order_relaxed);
    size_t w = writeIndex.load(std::memory_order_acquire);
    size_t availableItems = w - r;
    size_t toRead = std::min(count, availableItems);

    if (toRead == 0) {
      return 0;
    }

    size_t mask = CAPACITY - 1;
    for (int i = 0; i < static_cast<int>(toRead); ++i) {
      outData[i] = buffer[(r + i) & mask];
    }

    readIndex.store(r + toRead, std::memory_order_release);
    return toRead;
  }

  size_t available() const {
    size_t w = writeIndex.load(std::memory_order_acquire);
    size_t r = readIndex.load(std::memory_order_relaxed);
    return w - r;
  }

  size_t freeSpace() const {
    return CAPACITY - available();
  }

  size_t capacity() const {
    return CAPACITY;
  }

  void clear() {
    size_t w = writeIndex.load(std::memory_order_relaxed);
    readIndex.store(w, std::memory_order_release);
  }

private:
  alignas(64) std::atomic<size_t> writeIndex{0};
  alignas(64) std::atomic<size_t> readIndex{0};
  std::array<T, CAPACITY> buffer;
};

} // namespace nexrx
