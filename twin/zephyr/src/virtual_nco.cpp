/*
 * SPDX-License-Identifier: MIT
 * NexRx Digital Twin - Virtual NCO Implementation
 *
 * Copyright 2026 NexRx Project
 */

#include "virtual_nco.hpp"
#include "twin_transport.hpp"

#include <zephyr/logging/log.h>
#include <inttypes.h>

LOG_MODULE_REGISTER(virtual_nco, LOG_LEVEL_INF);

namespace nexrx {

void VirtualNco::initialize(TwinTransport* transport) {
    transport_ = transport;
    LOG_INF("Virtual NCO initialized with %d channels", NUM_CHANNELS);
}

bool VirtualNco::setFrequency(int channel, uint64_t freq_hz) {
    if (channel < 0 || channel >= NUM_CHANNELS) {
        LOG_ERR("Invalid NCO channel: %d", channel);
        return false;
    }

    freq_hz_[channel] = freq_hz;

    LOG_INF("NCO%d frequency set to %" PRIu64 " Hz", channel, freq_hz);

    // Send to orchestrator if connected
    if (transport_ && transport_->isRpcConnected()) {
        return transport_->sendSetFrequency(channel, freq_hz);
    }

    return true;
}

uint64_t VirtualNco::getFrequency(int channel) const {
    if (channel < 0 || channel >= NUM_CHANNELS) {
        return 0;
    }
    return freq_hz_[channel];
}

bool VirtualNco::setPhaseOffset(int channel, uint16_t phase_deg) {
    if (channel < 0 || channel >= NUM_CHANNELS) {
        return false;
    }

    phase_deg_[channel] = phase_deg % 360;

    LOG_INF("NCO%d phase offset set to %u deg", channel, phase_deg_[channel]);

    // Phase offset would be sent via RPC in a full implementation
    return true;
}

bool VirtualNco::setEnabled(int channel, bool enabled) {
    if (channel < 0 || channel >= NUM_CHANNELS) {
        return false;
    }

    enabled_[channel] = enabled;

    LOG_INF("NCO%d %s", channel, enabled ? "enabled" : "disabled");

    return true;
}

bool VirtualNco::isEnabled(int channel) const {
    if (channel < 0 || channel >= NUM_CHANNELS) {
        return false;
    }
    return enabled_[channel];
}

} // namespace nexrx
