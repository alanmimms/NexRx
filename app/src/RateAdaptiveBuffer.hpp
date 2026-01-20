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
        : config_(config)
        , buffer_(config.capacity)
    {
    }

    // Configure buffer (call before use, resizes buffer)
    void configure(const BufferConfig& config) {
        config_ = config;
        buffer_.resize(config.capacity);
        clear();
        stats_.reset();
    }

    //------------------------------------------------------------------
    // Producer API (called from network receive thread)
    //------------------------------------------------------------------

    /**
     * @brief Write a single sample to buffer
     * @return true if written, false if dropped
     */
    bool write(const T& sample) {
        size_t writePos = writePos_.load(std::memory_order_relaxed);
        size_t readPos = readPos_.load(std::memory_order_acquire);
        size_t nextWrite = (writePos + 1) % config_.capacity;

        if (nextWrite == readPos) {
            // Buffer full
            if (shouldDrop()) {
                stats_.dropsOverflow.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
            // Overwrite oldest (advance read pointer)
            readPos_.store((readPos + 1) % config_.capacity,
                          std::memory_order_release);
            stats_.dropsOverflow.fetch_add(1, std::memory_order_relaxed);
        }

        buffer_[writePos] = sample;
        writePos_.store(nextWrite, std::memory_order_release);
        stats_.samplesWritten.fetch_add(1, std::memory_order_relaxed);
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
        if (!config_.enableAdaptation) {
            return write(samples);
        }

        float fillRatio = getFillRatio();

        if (fillRatio > config_.highThreshold) {
            // Thin out samples instead of writing all
            float overAmount = fillRatio - config_.highThreshold;
            float dropRate = overAmount / (1.0f - config_.highThreshold);
            dropRate = std::clamp(dropRate, 0.0f, 0.5f);  // Max drop 50%

            size_t written = 0;
            float accumulator = 0.0f;

            for (const T& sample : samples) {
                accumulator += dropRate;
                if (accumulator >= 1.0f) {
                    // Skip this sample
                    accumulator -= 1.0f;
                    stats_.dropsOverflow.fetch_add(1, std::memory_order_relaxed);
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
        size_t readPos = readPos_.load(std::memory_order_relaxed);
        size_t writePos = writePos_.load(std::memory_order_acquire);

        if (readPos == writePos) {
            stats_.underruns.fetch_add(1, std::memory_order_relaxed);
            return false;
        }

        out = buffer_[readPos];
        readPos_.store((readPos + 1) % config_.capacity,
                       std::memory_order_release);
        stats_.samplesRead.fetch_add(1, std::memory_order_relaxed);
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
            size_t readPos = readPos_.load(std::memory_order_relaxed);
            size_t writePos = writePos_.load(std::memory_order_acquire);

            if (readPos == writePos) {
                out = T{};
                stats_.underruns.fetch_add(1, std::memory_order_relaxed);
            } else {
                out = buffer_[readPos];
                readPos_.store((readPos + 1) % config_.capacity,
                              std::memory_order_release);
                count++;
            }
        }

        stats_.samplesRead.fetch_add(count, std::memory_order_relaxed);
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
        if (!config_.enableAdaptation) {
            return readDirect(output);
        }

        float fillRatio = getFillRatio();

        if (fillRatio < config_.lowThreshold) {
            return readWithStretch(output, fillRatio);
        } else if (fillRatio > config_.highThreshold) {
            return readWithSkip(output, fillRatio);
        } else {
            return readDirect(output);
        }
    }

    //------------------------------------------------------------------
    // State queries
    //------------------------------------------------------------------

    size_t available() const {
        size_t readPos = readPos_.load(std::memory_order_relaxed);
        size_t writePos = writePos_.load(std::memory_order_acquire);

        if (writePos >= readPos) {
            return writePos - readPos;
        } else {
            return config_.capacity - readPos + writePos;
        }
    }

    size_t capacity() const { return config_.capacity; }

    float getFillRatio() const {
        return static_cast<float>(available()) / config_.capacity;
    }

    void clear() {
        readPos_.store(writePos_.load(std::memory_order_acquire),
                       std::memory_order_release);
    }

    const BufferStats& stats() const { return stats_; }
    BufferStats& stats() { return stats_; }

    const BufferConfig& config() const { return config_; }

    void setAdaptationEnabled(bool enabled) {
        config_.enableAdaptation = enabled;
    }

private:
    bool shouldDrop() const {
        return getFillRatio() > config_.highThreshold;
    }

    /**
     * @brief Read with linear interpolation to stretch output
     */
    size_t readWithStretch(std::span<T> output, float fillRatio) {
        stats_.stretchCount.fetch_add(1, std::memory_order_relaxed);

        size_t avail = available();
        if (avail < 2) {
            // Not enough for interpolation
            for (T& out : output) {
                out = T{};
            }
            stats_.underruns.fetch_add(output.size(), std::memory_order_relaxed);
            return 0;
        }

        // Calculate stretch ratio based on how low the buffer is
        float lowness = (config_.lowThreshold - fillRatio) / config_.lowThreshold;
        float stretchRatio = 1.0f + lowness * (config_.maxStretchRatio - 1.0f);
        stretchRatio = std::clamp(stretchRatio, 1.0f, config_.maxStretchRatio);

        // Step through input at slower rate
        float step = 1.0f / stretchRatio;
        float inputIndex = 0.0f;

        size_t readPos = readPos_.load(std::memory_order_relaxed);
        size_t produced = 0;

        for (T& out : output) {
            size_t idx0 = static_cast<size_t>(inputIndex);
            size_t idx1 = idx0 + 1;

            if (idx1 >= avail) {
                out = T{};
                stats_.underruns.fetch_add(1, std::memory_order_relaxed);
            } else {
                float frac = inputIndex - static_cast<float>(idx0);
                size_t pos0 = (readPos + idx0) % config_.capacity;
                size_t pos1 = (readPos + idx1) % config_.capacity;

                out = interpolate(buffer_[pos0], buffer_[pos1], frac);
                produced++;
            }

            inputIndex += step;
        }

        // Advance read pointer by actual samples consumed
        size_t consumed = std::min(static_cast<size_t>(inputIndex), avail - 1);
        readPos_.store((readPos + consumed) % config_.capacity,
                       std::memory_order_release);

        stats_.samplesRead.fetch_add(consumed, std::memory_order_relaxed);
        return produced;
    }

    /**
     * @brief Read with sample skipping to catch up
     */
    size_t readWithSkip(std::span<T> output, float fillRatio) {
        float overAmount = fillRatio - config_.highThreshold;
        float skipRate = overAmount / (1.0f - config_.highThreshold);
        skipRate = std::clamp(skipRate, 0.0f, 0.5f);  // Max skip 50%

        size_t count = 0;
        float accumulator = 0.0f;

        for (T& out : output) {
            size_t readPos = readPos_.load(std::memory_order_relaxed);
            size_t writePos = writePos_.load(std::memory_order_acquire);

            if (readPos == writePos) {
                out = T{};
                stats_.underruns.fetch_add(1, std::memory_order_relaxed);
            } else {
                out = buffer_[readPos];
                readPos_.store((readPos + 1) % config_.capacity,
                              std::memory_order_release);
                count++;

                // Occasionally skip an extra sample to catch up
                accumulator += skipRate;
                if (accumulator >= 1.0f && available() > 0) {
                    accumulator -= 1.0f;
                    size_t newReadPos = readPos_.load(std::memory_order_relaxed);
                    readPos_.store((newReadPos + 1) % config_.capacity,
                                  std::memory_order_release);
                    stats_.dropsOverflow.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }

        stats_.samplesRead.fetch_add(count, std::memory_order_relaxed);
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

    BufferConfig config_;
    std::vector<T> buffer_;

    // Cache-line aligned to prevent false sharing
    alignas(64) std::atomic<size_t> writePos_{0};
    alignas(64) std::atomic<size_t> readPos_{0};

    BufferStats stats_;
};

/**
 * @brief Helper to compute drops per second
 */
class DropRateTracker {
public:
    void update(uint64_t totalDrops, float currentTime) {
        float dt = currentTime - lastTime_;
        if (dt >= updateInterval_) {
            uint64_t delta = totalDrops - lastDrops_;
            dropsPerSecond_ = static_cast<float>(delta) / dt;
            lastDrops_ = totalDrops;
            lastTime_ = currentTime;
        }
    }

    float dropsPerSecond() const { return dropsPerSecond_; }

    void setUpdateInterval(float seconds) { updateInterval_ = seconds; }

private:
    float updateInterval_ = 1.0f;
    float lastTime_ = 0.0f;
    uint64_t lastDrops_ = 0;
    float dropsPerSecond_ = 0.0f;
};

} // namespace nexrx
