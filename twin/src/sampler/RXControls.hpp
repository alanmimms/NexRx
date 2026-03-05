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
  bool l701Short = false;    // true = bypass L701, use L702 only (220nH)

  // Capacitor enables (bit 0 = C701, bit 10 = C711)
  uint16_t capMask = 0;

  // Capacitor values in pF
  static constexpr std::array<uint16_t, 11> CAP_VALUES = {
    8, 15, 33, 68, 120, 250, 560, 1000, 2200, 3900, 8200
  };

  // Calculate total capacitance from mask
  [[nodiscard]] uint32_t totalCapacitancePF() const {
    uint32_t total = 0;
    for (int i = 0; i < 11; ++i) {
      if (capMask & (1 << i)) {
        total += CAP_VALUES[i];
      }
    }
    return total;
  }

  // Get resonant frequency for current L/C settings
  [[nodiscard]] double resonantFreqHz() const {
    double lNH = l701Short ? 220.0 : 1720.0;  // nH
    double cPF = static_cast<double>(totalCapacitancePF());
    if (cPF <= 0) {
      return 0;
    }

    // f = 1 / (2π√LC)
    double lH = lNH * 1e-9;
    double cF = cPF * 1e-12;
    return 1.0 / (2.0 * 3.14159265359 * std::sqrt(lH * cF));
  }

  // Preset for 14 MHz (20m band)
  // L702 only (220nH), C707(560) + C702(15) + C701(8) = 583pF
  static PreselectorConfig preset14MHz() {
    PreselectorConfig cfg;
    cfg.l701Short = true;
    cfg.capMask = (1 << 0) | (1 << 1) | (1 << 6);  // C701 + C702 + C707
    return cfg;
  }

  // Preset for 7 MHz (40m band)
  // L702 only (220nH), needs ~2300pF
  // C709(2200) + C705(120) = 2320pF
  static PreselectorConfig preset7MHz() {
    PreselectorConfig cfg;
    cfg.l701Short = true;
    cfg.capMask = (1 << 4) | (1 << 8);  // C705 + C709
    return cfg;
  }

  // Preset for 3.5 MHz (80m band)
  // Full inductor (1.72µH), needs ~1200pF
  // C708(1000) + C705(120) + C704(68) = 1188pF
  static PreselectorConfig preset3_5MHz() {
    PreselectorConfig cfg;
    cfg.l701Short = false;
    cfg.capMask = (1 << 3) | (1 << 4) | (1 << 7);  // C704 + C705 + C708
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
  bool atten3DB = false;
  bool atten6DB = false;
  bool atten12DB = false;
  bool atten24DB = false;

  // Calculate total attenuation in dB
  [[nodiscard]] uint8_t totalAttenDB() const {
    uint8_t total = 0;
    if (atten3DB) {
      total += 3;
    }
    if (atten6DB) {
      total += 6;
    }
    if (atten12DB) {
      total += 12;
    }
    if (atten24DB) {
      total += 24;
    }
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
struct VFOConfig {
  std::array<uint64_t, 3> freqHz = {14000000, 14000000, 14000000};

  // Phase offset in degrees for each QSD
  // QSD0/QSD1 use 0°, 90°, 180°, 270° (derived from freq)
  // QSD2 uses 0°, 60°, 120°, 180°, 240°, 300°

  // Preset for 14 MHz with 10kHz offset between QSD0 and QSD1
  static VFOConfig preset14MHz() {
    VFOConfig cfg;
    cfg.freqHz[0] = 13990000;  // f - 10kHz
    cfg.freqHz[1] = 14010000;  // f + 10kHz
    cfg.freqHz[2] = 14000000;  // f (sextature)
    return cfg;
  }

  // All QSDs at same frequency
  static VFOConfig atFreq(uint64_t freq) {
    VFOConfig cfg;
    cfg.freqHz[0] = freq;
    cfg.freqHz[1] = freq;
    cfg.freqHz[2] = freq;
    return cfg;
  }
};

//======================================================================
// Complete Receiver Configuration
//======================================================================
struct RXConfig {
  PreselectorConfig preselector;
  AttenuatorConfig attenuator;
  VFOConfig vfo;

  // Preset for 14 MHz operation
  static RXConfig preset14MHz() {
    RXConfig cfg;
    cfg.preselector = PreselectorConfig::preset14MHz();
    cfg.attenuator = AttenuatorConfig::bypass();
    cfg.vfo = VFOConfig::preset14MHz();
    return cfg;
  }
};

} // namespace nexrx
