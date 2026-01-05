/*
 * SPDX-License-Identifier: MIT
 * NexRx Digital Twin - Virtual NCO Interface
 *
 * Replaces the FPGA NCO hardware in simulation.
 * Sends frequency changes to the twin orchestrator which
 * updates the Xyce LO sources.
 *
 * Copyright 2026 NexRx Project
 */

#pragma once

#include <stdint.h>

namespace nexrx {

class TwinTransport;

class VirtualNco {
public:
    static constexpr int NUM_CHANNELS = 3;

    VirtualNco() = default;

    // Initialize with transport to orchestrator
    void initialize(TwinTransport* transport);

    // Set frequency for NCO channel (0-2)
    bool setFrequency(int channel, uint64_t freq_hz);

    // Get current frequency setting
    uint64_t getFrequency(int channel) const;

    // Set phase offset (for fine tuning)
    bool setPhaseOffset(int channel, uint16_t phase_deg);

    // Enable/disable NCO output
    bool setEnabled(int channel, bool enabled);

    // Check if NCO is enabled
    bool isEnabled(int channel) const;

private:
    TwinTransport* transport_ = nullptr;
    uint64_t freq_hz_[NUM_CHANNELS] = {14000000, 14000000, 14000000};
    uint16_t phase_deg_[NUM_CHANNELS] = {0, 0, 0};
    bool enabled_[NUM_CHANNELS] = {true, true, true};
};

} // namespace nexrx
