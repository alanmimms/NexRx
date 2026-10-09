#pragma once

#include "RingBuffer.hpp"
#include "transport/IQFrame.hpp"
#include "transport/IQPacketHeader.hpp"
#include "transport/Transport.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <algorithm>
#include <span>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__)
#include <immintrin.h>
#endif

namespace nexrx {

template <size_t CAPACITY = 65536>
class SamplePacer {
public:
  SamplePacer(RingBuffer<IQFrame, CAPACITY>& ring,
              StreamTransport* streamTransport,
              double sampleRateHz = 384000.0,
              double pacingIntervalMs = 10.0,
              size_t framesPerPacket = 128)
    : frameRing(ring)
    , transport(streamTransport)
    , sampleRate(sampleRateHz)
    , intervalMs(pacingIntervalMs)
    , packetFrames(framesPerPacket)
    , samplesPerTick(static_cast<size_t>(sampleRateHz * (pacingIntervalMs / 1000.0))) {
    readScratch.resize(std::max(samplesPerTick, packetFrames));
  }

  ~SamplePacer() {
    stop();
  }

  SamplePacer(const SamplePacer&) = delete;
  SamplePacer& operator=(const SamplePacer&) = delete;

  void setPacingIntervalMs(double ms) {
    intervalMs = ms;
    samplesPerTick = static_cast<size_t>(sampleRate * (ms / 1000.0));
    readScratch.resize(std::max(samplesPerTick, packetFrames));
  }

  void start() {
    if (running.load(std::memory_order_relaxed)) {
      return;
    }
    stopRequested.store(false, std::memory_order_relaxed);
    running.store(true, std::memory_order_relaxed);
    pacerThread = std::thread(&SamplePacer::pacerLoop, this);
  }

  void stop() {
    if (!running.load(std::memory_order_relaxed)) {
      return;
    }
    stopRequested.store(true, std::memory_order_release);
    notifySpace();
    if (pacerThread.joinable()) {
      pacerThread.join();
    }
    running.store(false, std::memory_order_relaxed);
  }

  bool isRunning() const {
    return running.load(std::memory_order_relaxed);
  }

  uint64_t getDeadlinesMissed() const {
    return deadlinesMissed.load(std::memory_order_relaxed);
  }

  uint64_t getPacketsSent() const {
    return packetsSentCount.load(std::memory_order_relaxed);
  }

  uint64_t getBytesSent() const {
    return bytesSentCount.load(std::memory_order_relaxed);
  }

  uint64_t getFramesSent() const {
    return framesSentCount.load(std::memory_order_relaxed);
  }

  void resetStats() {
    deadlinesMissed.store(0, std::memory_order_relaxed);
    packetsSentCount.store(0, std::memory_order_relaxed);
    bytesSentCount.store(0, std::memory_order_relaxed);
    framesSentCount.store(0, std::memory_order_relaxed);
  }

  void waitForSpace(size_t maxAllowedInRing, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(spaceMutex);
    cvSpace.wait_for(lock, timeout, [&]() {
      return frameRing.available() < maxAllowedInRing || stopRequested.load(std::memory_order_relaxed);
    });
  }

  void notifySpace() {
    cvSpace.notify_all();
  }

private:
  void pacerLoop() {
    auto pacerStartTime = std::chrono::steady_clock::now();
    uint64_t tickIndex = 0;
    int64_t intervalUs = static_cast<int64_t>(intervalMs * 1000.0);

    while (!stopRequested.load(std::memory_order_relaxed)) {
      auto scheduledWake = pacerStartTime + std::chrono::microseconds((tickIndex + 1) * intervalUs);
      auto nowP = std::chrono::steady_clock::now();

      if (scheduledWake > nowP) {
        auto waitDuration = scheduledWake - nowP;
        if (waitDuration > std::chrono::milliseconds(2)) {
          std::this_thread::sleep_for(waitDuration - std::chrono::milliseconds(1));
        }
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__)
        while (std::chrono::steady_clock::now() < scheduledWake) {
          _mm_pause();
        }
#else
        while (std::chrono::steady_clock::now() < scheduledWake) {
          std::this_thread::yield();
        }
#endif
      } else {
        deadlinesMissed.fetch_add(1, std::memory_order_relaxed);
        if (nowP - scheduledWake > std::chrono::milliseconds(200)) {
          pacerStartTime = nowP - std::chrono::microseconds((tickIndex + 1) * intervalUs);
        }
      }

      tickIndex++;

      // Pop samples for this tick
      size_t needed = samplesPerTick;
      size_t avail = frameRing.available();

      if (avail < needed) {
        // Buffer underflow: producer lagged behind
        deadlinesMissed.fetch_add(1, std::memory_order_relaxed);
      }

      size_t readCount = frameRing.read(readScratch.data(), needed);
      notifySpace();

      if (readCount > 0) {
        if (transport != nullptr) {
          size_t sent = 0;
          while (sent < readCount) {
            size_t batchSize = std::min(packetFrames, readCount - sent);
            std::span<const IQFrame> packetSpan(readScratch.data() + sent, batchSize);
            transport->writeBatch(packetSpan);

            packetsSentCount.fetch_add(1, std::memory_order_relaxed);
            bytesSentCount.fetch_add(sizeof(IQPacketHeader) + batchSize * sizeof(int32_t) * 2, std::memory_order_relaxed);
            framesSentCount.fetch_add(batchSize, std::memory_order_relaxed);

            sent += batchSize;
          }
        } else {
          size_t pkts = (readCount + packetFrames - 1) / packetFrames;
          packetsSentCount.fetch_add(pkts, std::memory_order_relaxed);
          bytesSentCount.fetch_add(pkts * sizeof(IQPacketHeader) + readCount * sizeof(int32_t) * 2, std::memory_order_relaxed);
          framesSentCount.fetch_add(readCount, std::memory_order_relaxed);
        }
      }
    }
  }

  RingBuffer<IQFrame, CAPACITY>& frameRing;
  StreamTransport* transport{nullptr};
  double sampleRate{384000.0};
  double intervalMs{10.0};
  size_t packetFrames{128};
  size_t samplesPerTick{3840};

  std::vector<IQFrame> readScratch;
  std::thread pacerThread;
  std::atomic<bool> running{false};
  std::atomic<bool> stopRequested{false};

  std::mutex spaceMutex;
  std::condition_variable cvSpace;

  alignas(64) std::atomic<uint64_t> deadlinesMissed{0};
  alignas(64) std::atomic<uint64_t> packetsSentCount{0};
  alignas(64) std::atomic<uint64_t> bytesSentCount{0};
  alignas(64) std::atomic<uint64_t> framesSentCount{0};
};

} // namespace nexrx
