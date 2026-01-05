/*
 * SPDX-License-Identifier: MIT
 * NexRx Digital Twin - I/Q Frame for Zephyr Firmware
 *
 * Standalone IQFrame definition without std library dependencies.
 * Must match the layout of transport/IQFrame.hpp exactly.
 *
 * For firmware (consumer side), we use volatile for the shared positions
 * since C++ doesn't support C11 _Atomic. The memory ordering is handled
 * by the orchestrator (producer) using proper atomics.
 *
 * Copyright 2026 NexRx Project
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

/* I/Q Sample - single channel */
struct IQSample {
    int32_t i;  /* In-phase (24-bit signed) */
    int32_t q;  /* Quadrature (24-bit signed) */
};

/* I/Q Frame - one sample from all three QSDs */
struct IQFrame {
    struct IQSample qsd[3];  /* QSD0, QSD1, QSD2 */
    uint64_t timestamp_ns;   /* Simulation timestamp */
    uint32_t sequence;       /* Frame sequence number */
    uint32_t flags;          /* Status flags */
};

/* Ring buffer header - must match SharedMemTransport layout */
struct IQRingBufferHeader {
    uint32_t magic;          /* 0x4E455852 = "NEXR" */
    uint32_t version;
    uint32_t capacity;
    uint32_t frame_size;
    char padding1[48];       /* Align to 64 bytes */

    volatile uint64_t write_pos;  /* Producer position */
    char padding2[56];       /* Cache line padding */

    volatile uint64_t read_pos;   /* Consumer position */
    char padding3[56];

    volatile uint64_t overruns;
    volatile uint64_t underruns;
    volatile uint64_t write_count;
    volatile uint64_t read_count;
    char padding4[32];
};

#define IQ_RING_MAGIC 0x4E455852
#define IQ_RING_VERSION 1
