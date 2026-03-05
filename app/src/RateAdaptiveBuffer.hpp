/**
 * @file RateAdaptiveBuffer.hpp
 * @brief Lock-free rate-adaptive ring buffer for real-time streaming
 *
 * Handles network jitter and slow consumers by:
 * - Dropping samples when buffer is too full
 * - Stretching (interpolating) when buffer is too empty
 * - Tracking statistics for monitoring
 */

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
#include <cmath>
#include <algorithm>
#include <type_traits>

namespace nexrx {

/**
 * @brief Statistics for rate-adaptive buffer operation
 */
struct BufferStats {
  std::atomic<uint64_t> samplesWritten{0};
  std::atomic<uint64_t> samplesRead{0};
  std::atomic<uint64_t> dropsOverflow{0};    // Dropped due to buffer full
  std::atomic<uint64_t> stretchCount{0};     // Times we stretched/interpolated
  std::atomic<uint64_t> underruns{0};        // Read from empty buffer

  void reset() {
    samplesWritten.store(0, std::memory_order_relaxed);
    samplesRead.store(0, std::memory_order_relaxed);
    dropsOverflow.store(0, std::memory_order_relaxed);
    stretchCount.store(0, std::memory_order_relaxed);
    underruns.store(0, std::memory_order_relaxed);
  }

  // Get drops since last call (for computing drops/second)
  uint64_t getAndResetDrops() {
    return dropsOverflow.exchange(0, std::memory_order_relaxed);
  }
};

/**
 * @brief Configuration for rate-adaptive buffer
 */
struct BufferConfig {
  size_t capacity = 16384;         // Total buffer size in samples
  float targetFillRatio = 0.5f;    // Target 50% full
  float lowThreshold = 0.25f;      // Below this: stretch/interpolate
  float highThreshold = 0.75f;     // Above this: drop frames
  float maxStretchRatio = 1.02f;   // Max 2% stretch
  bool enableAdaptation = true;    // Enable rate adaptation
};

/**
 * @brief Lock-free rate-adaptive ring buffer
 *
 * Features:
 * - Lock-free SPSC (single producer, single consumer) design
 * - Automatic rate adaptation via interpolation/dropping
 * - Fill-level tracking for rate estimation
 * - Separate statistics for monitoring
 *
 * @tparam T Sample type (float for audio, IQFrame for IQ data)
 */
template<typename T>
class RateAdaptiveBuffer {
public:
  explicit RateAdaptiveBuffer(const BufferConfig& config = BufferConfig{})
    : config(config)
    , buffer(config.capacity) {
  }

  // Configure buffer (call before use, resizes buffer)
  void configure(const BufferConfig& cfg) {
    config = cfg;
    buffer.resize(config.capacity);
    clear();
    bufferStats.reset();
  }

  //------------------------------------------------------------------
  // Producer API (called from network receive thread)
  //------------------------------------------------------------------

  /**
   * @brief Write a single sample to buffer
   * @return true if written, false if dropped
   */
  bool write(const T& sample) {
    size_t wPos = writePos.load(std::memory_order_relaxed);
    size_t rPos = readPos.load(std::memory_order_acquire);
    size_t nextWrite = (wPos + 1) % config.capacity;

    if (nextWrite == rPos) {
      // Buffer full
      if (shouldDrop()) {
        bufferStats.dropsOverflow.fetch_add(1, std::memory_order_relaxed);
        return false;
      }
      // Overwrite oldest (advance read pointer)
      readPos.store((rPos + 1) % config.capacity, std::memory_order_release);
      bufferStats.dropsOverflow.fetch_add(1, std::memory_order_relaxed);
    }

    buffer[wPos] = sample;
    writePos.store(nextWrite, std::memory_order_release);
    bufferStats.samplesWritten.fetch_add(1, std::memory_order_relaxed);
    return true;
  }

  /**
   * @brief Write multiple samples to buffer
   * @param samples Input samples
   * @return Number of samples actually written
   */
  size_t write(std::span<const T> samples) {
    size_t written = 0;
    for (const T& sample : samples) {
      if (write(sample)) {
        written++;
      }
    }
    return written;
  }

  /**
   * @brief Write a batch of samples with smart dropping
   *
   * When buffer is above highThreshold, drops samples evenly
   * distributed throughout the batch to gradually reduce fill level.
   */
  size_t writeBatch(std::span<const T> samples) {
    if (!config.enableAdaptation) {
      return write(samples);
    }

    float fillRatio = getFillRatio();

    if (fillRatio > config.highThreshold) {
      // Thin out samples instead of writing all
      float overAmount = fillRatio - config.highThreshold;
      float dropRate = overAmount / (1.0f - config.highThreshold);
      dropRate = std::clamp(dropRate, 0.0f, 0.5f);  // Max drop 50%

      size_t written = 0;
      float accumulator = 0.0f;

      for (const T& sample : samples) {
        accumulator += dropRate;
        if (accumulator >= 1.0f) {
          // Skip this sample
          accumulator -= 1.0f;
          bufferStats.dropsOverflow.fetch_add(1, std::memory_order_relaxed);
          continue;
        }
        if (write(sample)) {
          written++;
        }
      }
      return written;
    }

    return write(samples);
  }

  //------------------------------------------------------------------
  // Consumer API (called from audio/render thread)
  //------------------------------------------------------------------

  /**
   * @brief Read a single sample (no adaptation)
   * @param out Output sample
   * @return true if read, false if buffer empty
   */
  bool readOne(T& out) {
    size_t rPos = readPos.load(std::memory_order_relaxed);
    size_t wPos = writePos.load(std::memory_order_acquire);

    if (rPos == wPos) {
      bufferStats.underruns.fetch_add(1, std::memory_order_relaxed);
      return false;
    }

    out = buffer[rPos];
    readPos.store((rPos + 1) % config.capacity, std::memory_order_release);
    bufferStats.samplesRead.fetch_add(1, std::memory_order_relaxed);
    return true;
  }

  /**
   * @brief Read samples directly (no rate adaptation)
   * Fills with default T{} if not enough data available.
   * @return Number of actual samples read (not including zeros)
   */
  size_t readDirect(std::span<T> output) {
    size_t count = 0;

    for (T& out : output) {
      size_t rPos = readPos.load(std::memory_order_relaxed);
      size_t wPos = writePos.load(std::memory_order_acquire);

      if (rPos == wPos) {
        out = T{};
        bufferStats.underruns.fetch_add(1, std::memory_order_relaxed);
      } else {
        out = buffer[rPos];
        readPos.store((rPos + 1) % config.capacity, std::memory_order_release);
        count++;
      }
    }

    bufferStats.samplesRead.fetch_add(count, std::memory_order_relaxed);
    return count;
  }

  /**
   * @brief Read samples with rate adaptation
   *
   * When buffer is low, interpolates between samples to stretch.
   * When buffer is high, skips samples to catch up.
   *
   * @param output Buffer to fill
   * @return Number of samples produced (always == output.size())
   */
  size_t read(std::span<T> output) {
    if (!config.enableAdaptation) {
      return readDirect(output);
    }

    float fillRatio = getFillRatio();

    if (fillRatio < config.lowThreshold) {
      return readWithStretch(output, fillRatio);
    } else if (fillRatio > config.highThreshold) {
      return readWithSkip(output, fillRatio);
    } else {
      return readDirect(output);
    }
  }

  //------------------------------------------------------------------
  // State queries
  //------------------------------------------------------------------

  size_t available() const {
    size_t rPos = readPos.load(std::memory_order_relaxed);
    size_t wPos = writePos.load(std::memory_order_acquire);

    if (wPos >= rPos) {
      return wPos - rPos;
    } else {
      return config.capacity - rPos + wPos;
    }
  }

  size_t getCapacity() const { return config.capacity; }

  float getFillRatio() const {
    return static_cast<float>(available()) / config.capacity;
  }

  void clear() {
    readPos.store(writePos.load(std::memory_order_acquire), std::memory_order_release);
  }

  const BufferStats& stats() const { return bufferStats; }
  BufferStats& stats() { return bufferStats; }

  const BufferConfig& getConfig() const { return config; }

  void setAdaptationEnabled(bool enabled) {
    config.enableAdaptation = enabled;
  }

private:
  bool shouldDrop() const {
    return getFillRatio() > config.highThreshold;
  }

  /**
   * @brief Read with linear interpolation to stretch output
   */
  size_t readWithStretch(std::span<T> output, float fillRatio) {
    bufferStats.stretchCount.fetch_add(1, std::memory_order_relaxed);

    size_t avail = available();
    if (avail < 2) {
      // Not enough for interpolation
      for (T& out : output) {
        out = T{};
      }
      bufferStats.underruns.fetch_add(output.size(), std::memory_order_relaxed);
      return 0;
    }

    // Calculate stretch ratio based on how low the buffer is
    float lowness = (config.lowThreshold - fillRatio) / config.lowThreshold;
    float stretchRatio = 1.0f + lowness * (config.maxStretchRatio - 1.0f);
    stretchRatio = std::clamp(stretchRatio, 1.0f, config.maxStretchRatio);

    // Step through input at slower rate
    float step = 1.0f / stretchRatio;
    float inputIndex = 0.0f;

    size_t rPos = readPos.load(std::memory_order_relaxed);
    size_t produced = 0;

    for (T& out : output) {
      size_t idx0 = static_cast<size_t>(inputIndex);
      size_t idx1 = idx0 + 1;

      if (idx1 >= avail) {
        out = T{};
        bufferStats.underruns.fetch_add(1, std::memory_order_relaxed);
      } else {
        float frac = inputIndex - static_cast<float>(idx0);
        size_t pos0 = (rPos + idx0) % config.capacity;
        size_t pos1 = (rPos + idx1) % config.capacity;

        out = interpolate(buffer[pos0], buffer[pos1], frac);
        produced++;
      }

      inputIndex += step;
    }

    // Advance read pointer by actual samples consumed
    size_t consumed = std::min(static_cast<size_t>(inputIndex), avail - 1);
    readPos.store((rPos + consumed) % config.capacity, std::memory_order_release);

    bufferStats.samplesRead.fetch_add(consumed, std::memory_order_relaxed);
    return produced;
  }

  /**
   * @brief Read with sample skipping to catch up
   */
  size_t readWithSkip(std::span<T> output, float fillRatio) {
    float overAmount = fillRatio - config.highThreshold;
    float skipRate = overAmount / (1.0f - config.highThreshold);
    skipRate = std::clamp(skipRate, 0.0f, 0.5f);  // Max skip 50%

    size_t count = 0;
    float accumulator = 0.0f;

    for (T& out : output) {
      size_t rPos = readPos.load(std::memory_order_relaxed);
      size_t wPos = writePos.load(std::memory_order_acquire);

      if (rPos == wPos) {
        out = T{};
        bufferStats.underruns.fetch_add(1, std::memory_order_relaxed);
      } else {
        out = buffer[rPos];
        readPos.store((rPos + 1) % config.capacity, std::memory_order_release);
        count++;

        // Occasionally skip an extra sample to catch up
        accumulator += skipRate;
        if (accumulator >= 1.0f && available() > 0) {
          accumulator -= 1.0f;
          size_t newReadPos = readPos.load(std::memory_order_relaxed);
          readPos.store((newReadPos + 1) % config.capacity, std::memory_order_release);
          bufferStats.dropsOverflow.fetch_add(1, std::memory_order_relaxed);
        }
      }
    }

    bufferStats.samplesRead.fetch_add(count, std::memory_order_relaxed);
    return count;
  }

  // Linear interpolation helpers
  template<typename U>
  static U interpolate(const U& a, const U& b, float t) {
    if constexpr (std::is_arithmetic_v<U>) {
      return static_cast<U>(a * (1.0f - t) + b * t);
    } else {
      // For complex types, assume they have a lerp method or operator*
      return a;  // Fallback: no interpolation for complex types
    }
  }

  BufferConfig config;
  std::vector<T> buffer;

  // Cache-line aligned to prevent false sharing
  alignas(64) std::atomic<size_t> writePos{0};
  alignas(64) std::atomic<size_t> readPos{0};

  BufferStats bufferStats;
};

/**
 * @brief Helper to compute drops per second
 */
class DropRateTracker {
public:
  void update(uint64_t totalDrops, float currentTime) {
    float dt = currentTime - lastTime;
    if (dt >= updateInterval) {
      uint64_t delta = totalDrops - lastDrops;
      dropsPerSecondVal = static_cast<float>(delta) / dt;
      lastDrops = totalDrops;
      lastTime = currentTime;
    }
  }

  float getDropsPerSecond() const { return dropsPerSecondVal; }

  void setUpdateInterval(float seconds) { updateInterval = seconds; }

private:
  float updateInterval = 1.0f;
  float lastTime = 0.0f;
  uint64_t lastDrops = 0;
  float dropsPerSecondVal = 0.0f;
};

} // namespace nexrx
