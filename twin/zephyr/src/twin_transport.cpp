/*
 * SPDX-License-Identifier: MIT
 * NexRx Digital Twin - Transport Implementation for Zephyr Firmware
 *
 * Simplified transport using only POSIX shared memory for I/Q data.
 * RPC functionality is stubbed out for now since Zephyr's native_sim
 * doesn't include Unix domain sockets in its POSIX layer.
 *
 * Copyright 2026 NexRx Project
 */

#include "twin_transport.hpp"

#include <zephyr/logging/log.h>
#include <zephyr/posix/sys/mman.h>
#include <zephyr/posix/fcntl.h>
#include <zephyr/posix/unistd.h>
#include <zephyr/posix/sys/stat.h>

#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <errno.h>

LOG_MODULE_REGISTER(twin_transport, LOG_LEVEL_INF);

namespace nexrx {

TwinTransport::~TwinTransport() {
    shutdown();
}

bool TwinTransport::initialize(const char* shm_name, const char* rpc_path) {
    (void)rpc_path;  // RPC not implemented yet

    LOG_INF("Initializing twin transport");
    LOG_INF("  SHM: %s", shm_name);

    // Open shared memory for I/Q data
    shm_fd_ = shm_open(shm_name, O_RDONLY, 0);
    if (shm_fd_ < 0) {
        LOG_WRN("Failed to open shared memory: %s (errno=%d)", shm_name, errno);
        // Not fatal - orchestrator might not be running yet
        return false;
    }

    // Get size via fstat
    struct stat st;
    if (fstat(shm_fd_, &st) != 0) {
        LOG_ERR("Failed to stat shared memory");
        close(shm_fd_);
        shm_fd_ = -1;
        return false;
    }

    shm_size_ = st.st_size;
    shm_ptr_ = mmap(NULL, shm_size_, PROT_READ, MAP_SHARED, shm_fd_, 0);

    if (shm_ptr_ == MAP_FAILED) {
        LOG_ERR("Failed to mmap shared memory (errno=%d)", errno);
        close(shm_fd_);
        shm_fd_ = -1;
        shm_ptr_ = NULL;
        return false;
    }

    ring_header_ = static_cast<struct IQRingBufferHeader*>(shm_ptr_);
    ring_frames_ = reinterpret_cast<struct IQFrame*>(
        static_cast<uint8_t*>(shm_ptr_) + sizeof(struct IQRingBufferHeader));

    // Validate magic
    if (ring_header_->magic != IQ_RING_MAGIC) {
        LOG_ERR("Invalid shared memory magic: 0x%08x (expected 0x%08x)",
                ring_header_->magic, IQ_RING_MAGIC);
        munmap(shm_ptr_, shm_size_);
        close(shm_fd_);
        shm_fd_ = -1;
        shm_ptr_ = NULL;
        return false;
    }

    // Sync read position with current write position
    local_read_pos_ = ring_header_->read_pos;

    shm_connected_ = true;
    LOG_INF("Shared memory connected: %zu bytes, %u capacity",
            shm_size_, ring_header_->capacity);

    return true;
}

void TwinTransport::shutdown() {
    if (shm_ptr_ && shm_ptr_ != MAP_FAILED) {
        munmap(shm_ptr_, shm_size_);
        shm_ptr_ = NULL;
    }

    if (shm_fd_ >= 0) {
        close(shm_fd_);
        shm_fd_ = -1;
    }

    shm_connected_ = false;
    rpc_connected_ = false;
    ring_header_ = NULL;
    ring_frames_ = NULL;
}

bool TwinTransport::hasIqData() const {
    if (!shm_connected_ || !ring_header_) {
        return false;
    }

    uint64_t write_pos = ring_header_->write_pos;
    return write_pos != local_read_pos_;
}

bool TwinTransport::readIqFrame(struct IQFrame& frame) {
    if (!hasIqData()) {
        return false;
    }

    uint64_t write_pos = ring_header_->write_pos;

    if (local_read_pos_ == write_pos) {
        return false;
    }

    // Read frame from ring buffer
    size_t idx = local_read_pos_ % ring_header_->capacity;
    frame = ring_frames_[idx];

    // Advance read position
    ++local_read_pos_;

    return true;
}

size_t TwinTransport::availableFrames() const {
    if (!shm_connected_ || !ring_header_) {
        return 0;
    }

    uint64_t write_pos = ring_header_->write_pos;

    if (write_pos >= local_read_pos_) {
        return write_pos - local_read_pos_;
    }

    // Handle wrap (shouldn't happen with 64-bit counters)
    return 0;
}

bool TwinTransport::sendSetFrequency(int channel, uint64_t freq_hz) {
    (void)channel;
    (void)freq_hz;
    // RPC not implemented yet - would need Unix sockets or other IPC
    LOG_WRN("sendSetFrequency: RPC not implemented");
    return false;
}

bool TwinTransport::sendSetPreselector(uint16_t cap_mask, bool l701_short) {
    (void)cap_mask;
    (void)l701_short;
    LOG_WRN("sendSetPreselector: RPC not implemented");
    return false;
}

bool TwinTransport::sendSetAttenuator(uint8_t atten_db) {
    (void)atten_db;
    LOG_WRN("sendSetAttenuator: RPC not implemented");
    return false;
}

} // namespace nexrx
