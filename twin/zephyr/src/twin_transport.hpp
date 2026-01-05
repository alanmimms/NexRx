/*
 * SPDX-License-Identifier: MIT
 * NexRx Digital Twin - Transport Interface for Zephyr Firmware
 *
 * Provides communication with the twin orchestrator:
 * - Shared memory for I/Q data (high bandwidth)
 * - Unix socket for RPC control messages
 *
 * Copyright 2026 NexRx Project
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

// Standalone IQFrame header for Zephyr (no std library deps)
#include "iq_frame.hpp"

namespace nexrx {

class TwinTransport {
public:
    TwinTransport() = default;
    ~TwinTransport();

    // Initialize transport connections
    // shm_name: POSIX shared memory name for I/Q data
    // rpc_path: Unix socket path for control RPC
    bool initialize(const char* shm_name, const char* rpc_path);

    // Shutdown connections
    void shutdown();

    // Check connection status
    bool isConnected() const { return shm_connected_ && rpc_connected_; }
    bool isShmConnected() const { return shm_connected_; }
    bool isRpcConnected() const { return rpc_connected_; }

    // I/Q Data Interface (via shared memory)
    bool hasIqData() const;
    bool readIqFrame(struct IQFrame& frame);
    size_t availableFrames() const;

    // RPC Interface (via Unix socket)
    bool sendSetFrequency(int channel, uint64_t freq_hz);
    bool sendSetPreselector(uint16_t cap_mask, bool l701_short);
    bool sendSetAttenuator(uint8_t atten_db);

private:
    // Shared memory state
    int shm_fd_ = -1;
    void* shm_ptr_ = nullptr;
    size_t shm_size_ = 0;
    bool shm_connected_ = false;

    // RPC socket state
    int rpc_fd_ = -1;
    bool rpc_connected_ = false;

    // Cached pointers into shared memory
    struct IQRingBufferHeader* ring_header_ = nullptr;
    struct IQFrame* ring_frames_ = nullptr;

    // Local read position (consumer side)
    uint64_t local_read_pos_ = 0;
};

} // namespace nexrx
