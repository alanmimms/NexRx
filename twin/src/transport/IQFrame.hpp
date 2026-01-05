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
    IQSample(int32_t i_, int32_t q_) : i(i_), q(q_) {}

    // Convert to/from floating point (-1.0 to +1.0 range)
    static IQSample fromFloat(float i_f, float q_f) {
        constexpr float scale = 8388607.0f;  // 2^23 - 1
        return IQSample(
            static_cast<int32_t>(i_f * scale),
            static_cast<int32_t>(q_f * scale)
        );
    }

    void toFloat(float& i_f, float& q_f) const {
        constexpr float scale = 1.0f / 8388607.0f;
        i_f = static_cast<float>(i) * scale;
        q_f = static_cast<float>(q) * scale;
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
    uint64_t timestamp_ns;      // Nanoseconds since simulation/session start
    uint32_t sequence;          // Frame sequence number (wraps at 2^32)

    // Flags
    uint8_t flags;              // Reserved for future use
    uint8_t _padding[3];        // Align to 8-byte boundary

    IQFrame() : qsd{}, timestamp_ns(0), sequence(0), flags(0), _padding{} {}

    // Access helpers
    IQSample& operator[](size_t idx) { return qsd[idx]; }
    const IQSample& operator[](size_t idx) const { return qsd[idx]; }

    static constexpr size_t NUM_CHANNELS = 3;
    static constexpr size_t SAMPLE_RATE_HZ = 96000;
    static constexpr uint64_t SAMPLE_PERIOD_NS = 10416;  // 1e9 / 96000
};

// Verify size for shared memory alignment
static_assert(sizeof(IQSample) == 8, "IQSample must be 8 bytes");
static_assert(sizeof(IQFrame) == 40, "IQFrame must be 40 bytes");
static_assert(alignof(IQFrame) <= 8, "IQFrame alignment must be <= 8");

//======================================================================
// Ring buffer header for shared memory
// Placed at the start of the shared memory region
//======================================================================
struct IQRingBufferHeader {
    uint32_t magic;             // Magic number for validation (0x4E585251 = "NXRQ")
    uint32_t version;           // Protocol version
    uint32_t capacity;          // Number of frames in buffer
    uint32_t frame_size;        // sizeof(IQFrame), for validation

    // Producer (writer) state
    alignas(64) uint64_t write_pos;     // Next write position (atomic)
    uint64_t write_count;               // Total frames written

    // Consumer (reader) state
    alignas(64) uint64_t read_pos;      // Next read position (atomic)
    uint64_t read_count;                // Total frames read

    // Statistics
    alignas(64) uint64_t overruns;      // Frames lost due to full buffer
    uint64_t underruns;                 // Read attempts on empty buffer

    static constexpr uint32_t MAGIC = 0x4E585251;  // "NXRQ"
    static constexpr uint32_t VERSION = 1;

    bool isValid() const {
        return magic == MAGIC &&
               version == VERSION &&
               frame_size == sizeof(IQFrame);
    }
};

// Cache line alignment for lock-free operation
static_assert(offsetof(IQRingBufferHeader, write_pos) % 64 == 0, "write_pos must be cache-aligned");
static_assert(offsetof(IQRingBufferHeader, read_pos) % 64 == 0, "read_pos must be cache-aligned");
static_assert(offsetof(IQRingBufferHeader, overruns) % 64 == 0, "stats must be cache-aligned");

} // namespace nexrx
