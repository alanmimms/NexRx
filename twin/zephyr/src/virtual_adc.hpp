/*
 * SPDX-License-Identifier: MIT
 * NexRx Digital Twin - Virtual ADC Interface
 *
 * Replaces the physical ADC hardware in simulation.
 * Receives I/Q samples from the twin orchestrator via shared memory.
 *
 * Copyright 2026 NexRx Project
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

#include "iq_frame.hpp"

namespace nexrx {

class TwinTransport;

class VirtualAdc {
public:
    static constexpr int NUM_CHANNELS = 6;  // I0, Q0, I1, Q1, I2, Q2
    static constexpr uint32_t SAMPLE_RATE = 96000;
    static constexpr int RESOLUTION_BITS = 24;

    VirtualAdc() = default;

    // Initialize with transport to orchestrator
    void initialize(TwinTransport* transport);

    // Check if I/Q data is available
    bool hasData() const;

    // Read next I/Q frame (blocks if no data)
    struct IQFrame readFrame();

    // Read frame without blocking (returns false if no data)
    bool tryReadFrame(struct IQFrame& frame);

    // Get number of available frames
    size_t available() const;

    // Get total frame count since start
    uint64_t frameCount() const { return frame_count_; }

    // Get sample rate
    uint32_t sampleRate() const { return SAMPLE_RATE; }

    // Get resolution in bits
    int resolution() const { return RESOLUTION_BITS; }

private:
    TwinTransport* transport_ = nullptr;
    uint64_t frame_count_ = 0;
};

} // namespace nexrx
