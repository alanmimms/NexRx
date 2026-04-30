// NexRx Digital Twin - Receiver Control Stubs
//
// Default configurations for filters, attenuator, and VFO.
// These will eventually be controlled by the Zephyr STM32 simulation.
//
// Copyright 2026 NexRx Project - MIT License

#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace nexrx {

//======================================================================
// Filter Bank Configuration
//
// AM Reject HPF (bypassed or engaged)
// BPF Bank (one-hot selection, 0=bypass, 1-5=bands)
//======================================================================
struct FilterBankConfig {
  bool hpfBypass = false;
  int bpfIndex = 0; // 0: Bypass, 1: 1.8-3.4, 2: 3.2-7.5, 3: 7.3-14.5, 4: 14.3-22, 5: 21.8-30

  // Preset for 14 MHz (20m band, band 3)
  static FilterBankConfig preset14MHz() {
    FilterBankConfig cfg;
    cfg.hpfBypass = false;
    cfg.bpfIndex = 3;
    return cfg;
  }

  // Preset for 7 MHz (40m band, band 2)
  static FilterBankConfig preset7MHz() {
    FilterBankConfig cfg;
    cfg.hpfBypass = false;
    cfg.bpfIndex = 2;
    return cfg;
  }

  // Preset for 3.5 MHz (80m band, band 2)
  static FilterBankConfig preset3_5MHz() {
    FilterBankConfig cfg;
    cfg.hpfBypass = false;
    cfg.bpfIndex = 2;
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
  FilterBankConfig filters;
  AttenuatorConfig attenuator;
  VFOConfig vfo;

  // Preset for 14 MHz operation
  static RXConfig preset14MHz() {
    RXConfig cfg;
    cfg.filters = FilterBankConfig::preset14MHz();
    cfg.attenuator = AttenuatorConfig::bypass();
    cfg.vfo = VFOConfig::preset14MHz();
    return cfg;
  }
};

} // namespace nexrx