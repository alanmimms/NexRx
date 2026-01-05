/*
 * SPDX-License-Identifier: MIT
 * NexRx Digital Twin - Zephyr Firmware Main
 *
 * Entry point for the STM32 firmware running on native_sim.
 * Connects to the twin orchestrator for I/Q data and control.
 *
 * Copyright 2026 NexRx Project
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <inttypes.h>
#include <stdlib.h>

#include "twin_transport.hpp"
#include "virtual_nco.hpp"
#include "virtual_adc.hpp"

LOG_MODULE_REGISTER(nexrx_main, LOG_LEVEL_INF);

namespace {

// Twin transport for communication with orchestrator
nexrx::TwinTransport g_transport;

// Virtual peripherals
nexrx::VirtualNco g_nco;
nexrx::VirtualAdc g_adc;

// Current VFO frequencies (Hz)
uint64_t g_vfo_freq[3] = {14000000, 14000000, 14000000};

// Processing thread
K_THREAD_STACK_DEFINE(process_stack, 4096);
struct k_thread process_thread;

void process_iq_thread(void* p1, void* p2, void* p3) {
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    LOG_INF("I/Q processing thread started");

    while (true) {
        // Read I/Q frame from shared memory
        if (g_adc.hasData()) {
            auto frame = g_adc.readFrame();

            // Process I/Q data here
            // In real firmware, this would:
            // 1. Apply digital filtering
            // 2. Combine QSD outputs for image rejection
            // 3. Decimate to audio rate
            // 4. Send to USB audio class

            // For now, just count frames
            static uint64_t frame_count = 0;
            if (++frame_count % 9600 == 0) {  // Every 100ms
                LOG_INF("Processed %" PRIu64 " frames", frame_count);
            }
        } else {
            // No data available, sleep briefly
            k_msleep(1);
        }
    }
}

} // anonymous namespace

// Shell commands for debugging
static int cmd_status(const struct shell* sh, size_t argc, char** argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    shell_print(sh, "NexRx Twin Firmware Status");
    shell_print(sh, "  Transport: %s", g_transport.isConnected() ? "connected" : "disconnected");
    shell_print(sh, "  VFO0: %" PRIu64 " Hz", g_vfo_freq[0]);
    shell_print(sh, "  VFO1: %" PRIu64 " Hz", g_vfo_freq[1]);
    shell_print(sh, "  VFO2: %" PRIu64 " Hz", g_vfo_freq[2]);
    shell_print(sh, "  ADC frames: %" PRIu64 "", g_adc.frameCount());

    return 0;
}

static int cmd_freq(const struct shell* sh, size_t argc, char** argv) {
    if (argc < 3) {
        shell_print(sh, "Usage: freq <vfo 0-2> <freq_hz>");
        return -EINVAL;
    }

    int vfo = atoi(argv[1]);
    uint64_t freq = strtoull(argv[2], nullptr, 10);

    if (vfo < 0 || vfo > 2) {
        shell_print(sh, "Invalid VFO: %d (must be 0-2)", vfo);
        return -EINVAL;
    }

    g_vfo_freq[vfo] = freq;
    g_nco.setFrequency(vfo, freq);

    shell_print(sh, "VFO%d set to %" PRIu64 " Hz", vfo, freq);
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(nexrx_cmds,
    SHELL_CMD(status, NULL, "Show firmware status", cmd_status),
    SHELL_CMD(freq, NULL, "Set VFO frequency", cmd_freq),
    SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(nexrx, &nexrx_cmds, "NexRx commands", NULL);

int main(void) {
    LOG_INF("NexRx Digital Twin Firmware starting...");
    LOG_INF("Build: %s %s", __DATE__, __TIME__);

    // Initialize transport to orchestrator
    if (!g_transport.initialize("/nexrx_iq", "/tmp/nexrx_rpc.sock")) {
        LOG_ERR("Failed to initialize transport");
        // Continue anyway - we can still test shell commands
    }

    // Initialize virtual peripherals
    g_nco.initialize(&g_transport);
    g_adc.initialize(&g_transport);

    // Set default frequencies
    for (int i = 0; i < 3; ++i) {
        g_nco.setFrequency(i, g_vfo_freq[i]);
    }

    // Start I/Q processing thread
    k_thread_create(&process_thread, process_stack,
                    K_THREAD_STACK_SIZEOF(process_stack),
                    process_iq_thread, nullptr, nullptr, nullptr,
                    K_PRIO_COOP(7), 0, K_NO_WAIT);
    k_thread_name_set(&process_thread, "iq_process");

    LOG_INF("Firmware initialized, entering shell");

    // Main thread just runs the shell
    // The processing happens in the process_iq_thread
    return 0;
}
