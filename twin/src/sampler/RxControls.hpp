// NexRx Digital Twin - Receiver Control Stubs
//
// Default configurations for preselector, attenuator, and VFO.
// These will eventually be controlled by the Zephyr STM32 simulation.
//
// Copyright 2026 NexRx Project - MIT License

#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace nexrx {

//======================================================================
// Preselector Configuration
//
// Shunt LC tank with switched L and C:
//   L701 = 1.5µH (can be shorted for high frequencies)
//   L702 = 220nH (always in circuit)
//   C701-C711 = Switched capacitor bank (8pF to 8.2nF)
//======================================================================
struct PreselectorConfig {
    bool l701_short = false;    // true = bypass L701, use L702 only (220nH)

    // Capacitor enables (bit 0 = C701, bit 10 = C711)
    uint16_t cap_mask = 0;

    // Capacitor values in pF
    static constexpr std::array<uint16_t, 11> CAP_VALUES = {
        8, 15, 33, 68, 120, 250, 560, 1000, 2200, 3900, 8200
    };

    // Calculate total capacitance from mask
    [[nodiscard]] uint32_t totalCapacitancePf() const {
        uint32_t total = 0;
        for (int i = 0; i < 11; ++i) {
            if (cap_mask & (1 << i)) {
                total += CAP_VALUES[i];
            }
        }
        return total;
    }

    // Get resonant frequency for current L/C settings
    [[nodiscard]] double resonantFreqHz() const {
        double l_nH = l701_short ? 220.0 : 1720.0;  // nH
        double c_pF = static_cast<double>(totalCapacitancePf());
        if (c_pF <= 0) return 0;

        // f = 1 / (2π√LC)
        double l_H = l_nH * 1e-9;
        double c_F = c_pF * 1e-12;
        return 1.0 / (2.0 * 3.14159265359 * std::sqrt(l_H * c_F));
    }

    // Preset for 14 MHz (20m band)
    // L702 only (220nH), C707(560) + C702(15) + C701(8) = 583pF
    static PreselectorConfig preset14MHz() {
        PreselectorConfig cfg;
        cfg.l701_short = true;
        cfg.cap_mask = (1 << 0) | (1 << 1) | (1 << 6);  // C701 + C702 + C707
        return cfg;
    }

    // Preset for 7 MHz (40m band)
    // L702 only (220nH), needs ~2300pF
    // C709(2200) + C705(120) = 2320pF
    static PreselectorConfig preset7MHz() {
        PreselectorConfig cfg;
        cfg.l701_short = true;
        cfg.cap_mask = (1 << 4) | (1 << 8);  // C705 + C709
        return cfg;
    }

    // Preset for 3.5 MHz (80m band)
    // Full inductor (1.72µH), needs ~1200pF
    // C708(1000) + C705(120) + C704(68) = 1188pF
    static PreselectorConfig preset3_5MHz() {
        PreselectorConfig cfg;
        cfg.l701_short = false;
        cfg.cap_mask = (1 << 3) | (1 << 4) | (1 << 7);  // C704 + C705 + C708
        return cfg;
    }
};

//======================================================================
// Attenuator Configuration
//
// Four switchable Pi-attenuator stages: 3dB, 6dB, 12dB, 24dB
// Each stage is either bypassed (0dB) or engaged
//======================================================================
struct AttenuatorConfig {
    bool atten_3db = false;
    bool atten_6db = false;
    bool atten_12db = false;
    bool atten_24db = false;

    // Calculate total attenuation in dB
    [[nodiscard]] uint8_t totalAttenDb() const {
        uint8_t total = 0;
        if (atten_3db)  total += 3;
        if (atten_6db)  total += 6;
        if (atten_12db) total += 12;
        if (atten_24db) total += 24;
        return total;
    }

    // No attenuation
    static AttenuatorConfig bypass() {
        return AttenuatorConfig{false, false, false, false};
    }

    // Maximum attenuation (45dB)
    static AttenuatorConfig maximum() {
        return AttenuatorConfig{true, true, true, true};
    }
};

//======================================================================
// VFO Configuration
//
// Three independent NCOs for the three QSDs.
// QSD0/QSD1: 4-phase quadrature (standard I/Q)
// QSD2: 6-phase sextature (for image rejection)
//======================================================================
struct VfoConfig {
    std::array<uint64_t, 3> freq_hz = {14000000, 14000000, 14000000};

    // Phase offset in degrees for each QSD
    // QSD0/QSD1 use 0°, 90°, 180°, 270° (derived from freq)
    // QSD2 uses 0°, 60°, 120°, 180°, 240°, 300°

    // Preset for 14 MHz with 10kHz offset between QSD0 and QSD1
    static VfoConfig preset14MHz() {
        VfoConfig cfg;
        cfg.freq_hz[0] = 13990000;  // f - 10kHz
        cfg.freq_hz[1] = 14010000;  // f + 10kHz
        cfg.freq_hz[2] = 14000000;  // f (sextature)
        return cfg;
    }

    // All QSDs at same frequency
    static VfoConfig atFreq(uint64_t freq) {
        VfoConfig cfg;
        cfg.freq_hz[0] = freq;
        cfg.freq_hz[1] = freq;
        cfg.freq_hz[2] = freq;
        return cfg;
    }
};

//======================================================================
// Complete Receiver Configuration
//======================================================================
struct RxConfig {
    PreselectorConfig preselector;
    AttenuatorConfig attenuator;
    VfoConfig vfo;

    // Preset for 14 MHz operation
    static RxConfig preset14MHz() {
        RxConfig cfg;
        cfg.preselector = PreselectorConfig::preset14MHz();
        cfg.attenuator = AttenuatorConfig::bypass();
        cfg.vfo = VfoConfig::preset14MHz();
        return cfg;
    }
};

} // namespace nexrx
