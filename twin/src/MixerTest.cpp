// NexRx Digital Twin - Mixer Test
//
// Verifies that the analytic complex mixing logic correctly 
// downconverts RF signals to baseband without aliasing.
//
// Copyright 2026 NexRx Project - MIT License

#include "stimulus/ToneGenerator.hpp"
#include "stimulus/MorseGenerator.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>

using namespace nexrx;

void testComplexMixing() {
    std::cout << "=== Complex Mixer High-Fidelity Test ===" << std::endl;

    // Signal: 14.075 MHz (Beacon)
    double f_rf = 14.075e6;
    auto tone = ToneGenerator::cw(f_rf, 1.0);

    // VFO: 14.215 MHz (App tune frequency)
    double f_vfo = 14.215e6;

    // We expect baseband frequency to be f_rf - f_vfo = -140 kHz
    double expected_f_bb = f_rf - f_vfo;

    std::cout << "Signal: " << f_rf / 1e6 << " MHz" << std::endl;
    std::cout << "VFO:    " << f_vfo / 1e6 << " MHz" << std::endl;
    std::cout << "Expected Baseband: " << expected_f_bb / 1e3 << " kHz" << std::endl;

    // Simulation parameters (same as twin)
    constexpr double fs = 480000.0;
    constexpr double T = 1.0 / fs;
    constexpr int N = 480; // 1ms of simulation

    double phase_error_sum = 0;
    
    for (int i = 0; i < N; ++i) {
        double t = i * T;
        
        // 1. Get RF IQ
        double rf_i, rf_q;
        tone.getRfIQ(t, rf_i, rf_q);

        // 2. Mix with VFO
        double lo_p = 2.0 * M_PI * f_vfo * t;
        double cos_lo = std::cos(lo_p);
        double sin_lo = std::sin(lo_p);

        // Downconvert: (rf_i + j*rf_q) * exp(-j * lo_p)
        double bb_i = rf_i * cos_lo + rf_q * sin_lo;
        double bb_q = rf_q * cos_lo - rf_i * sin_lo;

        // 3. Verify against expected baseband
        double expected_phase = 2.0 * M_PI * expected_f_bb * t;
        double expected_i = std::cos(expected_phase);
        double expected_q = std::sin(expected_phase);

        double diff_i = bb_i - expected_i;
        double diff_q = bb_q - expected_q;
        phase_error_sum += std::sqrt(diff_i*diff_i + diff_q*diff_q);
    }

    std::cout << "Average Error: " << phase_error_sum / N << std::endl;
    if (phase_error_sum / N < 1e-10) {
        std::cout << "SUCCESS: Complex mixing is mathematically exact." << std::endl;
    } else {
        std::cout << "FAILURE: Mixing error too high!" << std::endl;
    }
}

void testMorseAliasingFix() {
    std::cout << "\n=== Morse Generator Aliasing Fix Test ===" << std::endl;

    double f_rf = 14.075e6;
    MorseGenerator morse(f_rf, 1.0, "TEST", 20);

    // Sample at 480 kHz. 
    // If it's fixed, getRfIQ should return the 14.075 MHz phase.
    // If it's aliased, it will return the 155 kHz phase.
    
    constexpr double fs = 480000.0;
    double t = 1.0 / fs; // first sample
    
    double rf_i, rf_q;
    morse.getRfIQ(t, rf_i, rf_q);

    double actual_phase = std::atan2(rf_q, rf_i);
    double expected_phase = std::fmod(2.0 * M_PI * f_rf * t, 2.0 * M_PI);
    if (expected_phase > M_PI) expected_phase -= 2.0 * M_PI;

    std::cout << "Expected Phase at T=1/fs: " << expected_phase << std::endl;
    std::cout << "Actual Phase at T=1/fs:   " << actual_phase << std::endl;

    if (std::abs(actual_phase - expected_phase) < 1e-10) {
        std::cout << "SUCCESS: MorseGenerator supports absolute RF frequencies." << std::endl;
    } else {
        std::cout << "FAILURE: MorseGenerator is still aliasing!" << std::endl;
    }
}

int main() {
    testComplexMixing();
    testMorseAliasingFix();
    return 0;
}
