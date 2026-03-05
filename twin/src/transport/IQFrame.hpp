// NexRx Digital Twin - I/Q Frame Structure
//
// Lightweight struct for I/Q sample data from three QSDs.
// Designed for efficient shared memory transfer.
//
// Copyright 2026 NexRx Project - MIT License

#pragma once

#include <cstdint>
#include <cstddef>
#include <array>

namespace nexrx {

//======================================================================
// I/Q Sample (single channel)
// 24-bit signed values stored in 32-bit integers
//======================================================================
struct IQSample {
  int32_t i;      // In-phase component (24-bit signed, -8388608 to 8388607)
  int32_t q;      // Quadrature component

  IQSample() : i(0), q(0) {}
  IQSample(int32_t iIn, int32_t qIn) : i(iIn), q(qIn) {}

  // Convert to/from floating point (-1.0 to +1.0 range)
  static IQSample fromFloat(float iF, float qF) {
    constexpr float scale = 8388607.0f;
    return IQSample(
      static_cast<int32_t>(iF * scale),
      static_cast<int32_t>(qF * scale)
    );
  }

  void toFloat(float& iF, float& qF) const {
    // 24-bit range is +/- 8388607
    constexpr float scale = 1.0f / 8388607.0f;
    iF = static_cast<float>(i) * scale;
    qF = static_cast<float>(q) * scale;
  }

  // Magnitude squared (for signal strength)
  int64_t magnitudeSquared() const {
    return static_cast<int64_t>(i) * i + static_cast<int64_t>(q) * q;
  }
};

//======================================================================
// I/Q Frame (all three QSDs)
// One frame per sample period (96kHz = 10.4μs per frame)
//======================================================================
struct IQFrame {
  // Samples from each QSD
  IQSample qsd[3];

  // Timing information
  uint64_t timestampNS;      // Nanoseconds since simulation/start
  uint32_t sequence;          // Frame sequence number

  // Flags
  uint8_t flags;              // Reserved
  uint8_t padding[3];        // Alignment

  IQFrame() : qsd{}, timestampNS(0), sequence(0), flags(0), padding{} {}

  // Access helpers
  IQSample& operator[](size_t idx) { return qsd[idx]; }
  const IQSample& operator[](size_t idx) const { return qsd[idx]; }

  static constexpr size_t NUM_CHANNELS = 3;
  static constexpr size_t SAMPLE_RATE_HZ = 96000;
  static constexpr uint64_t SAMPLE_PERIOD_NS = 10416;
};

// Verify size for shared memory alignment
static_assert(sizeof(IQSample) == 8, "IQSample must be 8 bytes");
static_assert(sizeof(IQFrame) == 40, "IQFrame must be 40 bytes");

//======================================================================
// Ring buffer header for shared memory
//======================================================================
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324)
#endif
struct IQRingBufferHeader {
  uint32_t magic;             // Magic: 0x4E585251 ("NXRQ")
  uint32_t version;           // Protocol version
  uint32_t capacity;          // Frames in buffer
  uint32_t frameSize;        // sizeof(IQFrame)

  // Producer state
  alignas(64) uint64_t writePos;
  uint64_t writeCount;

  // Consumer state
  alignas(64) uint64_t readPos;
  uint64_t readCount;

  // Statistics
  alignas(64) uint64_t overruns;
  uint64_t underruns;

  static constexpr uint32_t MAGIC = 0x4E585251;
  static constexpr uint32_t VERSION = 1;

  bool isValid() const {
    return magic == MAGIC && version == VERSION && frameSize == sizeof(IQFrame);
  }
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace nexrx
