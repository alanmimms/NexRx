/*
 * SPDX-License-Identifier: MIT
 * NexRx Digital Twin - Virtual ADC Implementation
 *
 * Copyright 2026 NexRx Project
 */

#include "virtual_adc.hpp"
#include "twin_transport.hpp"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(virtual_adc, LOG_LEVEL_INF);

namespace nexrx {

void VirtualAdc::initialize(TwinTransport* transport) {
    transport_ = transport;
    frame_count_ = 0;

    LOG_INF("Virtual ADC initialized");
    LOG_INF("  Channels: %d", NUM_CHANNELS);
    LOG_INF("  Sample rate: %u Hz", SAMPLE_RATE);
    LOG_INF("  Resolution: %d bits", RESOLUTION_BITS);
}

bool VirtualAdc::hasData() const {
    if (!transport_) {
        return false;
    }
    return transport_->hasIqData();
}

struct IQFrame VirtualAdc::readFrame() {
    struct IQFrame frame{};

    if (!transport_) {
        return frame;
    }

    // Block until data is available
    while (!transport_->hasIqData()) {
        k_msleep(1);
    }

    if (transport_->readIqFrame(frame)) {
        ++frame_count_;
    }

    return frame;
}

bool VirtualAdc::tryReadFrame(struct IQFrame& frame) {
    if (!transport_ || !transport_->hasIqData()) {
        return false;
    }

    if (transport_->readIqFrame(frame)) {
        ++frame_count_;
        return true;
    }

    return false;
}

size_t VirtualAdc::available() const {
    if (!transport_) {
        return 0;
    }
    return transport_->availableFrames();
}

} // namespace nexrx
