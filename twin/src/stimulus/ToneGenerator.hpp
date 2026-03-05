// NexRx Digital Twin - Tone Generator
//
// Generates single or multi-tone CW signals for RF testing.
// Supports amplitude, frequency, and phase control per tone.
//
// Copyright 2026 NexRx Project - MIT License

#pragma once

#include "AntennaStimulus.hpp"

#include <cmath>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>

namespace nexrx {

//======================================================================
// Single Tone Parameters
//======================================================================
struct ToneParams {
    double frequency_hz;    // Tone frequency
    double amplitudeV;     // Peak amplitude in volts
    double phase_deg = 0.0; // Initial phase in degrees

    ToneParams(double freq, double amp, double phase = 0.0)
        : frequency_hz(freq), amplitudeV(amp), phase_deg(phase) {}
};

//======================================================================
// Tone Generator
//
// Generates sum of sinusoidal tones:
//   V(t) = sum_i [ A_i * sin(2*pi*f_i*t + phi_i) ]
//
// Use cases:
//   - Single CW carrier for mixer testing
//   - Two-tone for IMD testing
//   - Multiple tones for selectivity testing
//======================================================================
class ToneGenerator : public AntennaStimulus {
public:
    ToneGenerator() = default;

    // Single tone convenience constructor
    explicit ToneGenerator(double frequency_hz, double amplitudeV = 0.1, double phase_deg = 0.0) {
        addTone(frequency_hz, amplitudeV, phase_deg);
    }

    // Add a tone to the generator
    void addTone(double frequency_hz, double amplitudeV, double phase_deg = 0.0) {
        tones_.emplace_back(frequency_hz, amplitudeV, phase_deg);
    }

    void addTone(const ToneParams& params) {
        tones_.push_back(params);
    }

    // Clear all tones
    void clear() {
        tones_.clear();
    }

    [[nodiscard]] double getSample(double timeS) const override {
        double sum = 0.0;
        for (const auto& tone : tones_) {
            double phase_rad = tone.phase_deg * M_PI / 180.0;
            sum += tone.amplitudeV * std::sin(2.0 * M_PI * tone.frequency_hz * timeS + phase_rad);
        }
        return sum;
    }

    // Get analytic RF signal (complex envelope at carrier)
    // Returns RF I/Q without any LO knowledge - QSD layer does mixing
    void getRfIQ(double timeS, double& out_i, double& out_q) const override {
        out_i = out_q = 0.0;
        for (const auto& tone : tones_) {
            double phase_rad = tone.phase_deg * M_PI / 180.0;
            double phase = 2.0 * M_PI * tone.frequency_hz * timeS + phase_rad;
            out_i += tone.amplitudeV * std::cos(phase);
            out_q += tone.amplitudeV * std::sin(phase);
        }
    }

    [[nodiscard]] double carrierFrequency() const override {
        // Return first tone frequency as primary carrier
        return tones_.empty() ? 0.0 : tones_[0].frequency_hz;
    }

    [[nodiscard]] std::string description() const override {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3);

        if (tones_.empty()) {
            return "ToneGenerator(none)";
        } else if (tones_.size() == 1) {
            oss << "Tone(" << tones_[0].frequency_hz / 1e6 << "MHz, "
                << tones_[0].amplitudeV * 1000 << "mV)";
        } else {
            oss << "MultiTone[";
            for (size_t i = 0; i < tones_.size(); ++i) {
                if (i > 0) oss << ", ";
                oss << tones_[i].frequency_hz / 1e6 << "MHz";
            }
            oss << "]";
        }
        return oss.str();
    }

    // Get number of tones
    [[nodiscard]] size_t toneCount() const { return tones_.size(); }

    // Get tone parameters
    [[nodiscard]] const ToneParams& getTone(size_t index) const { return tones_.at(index); }

    //------------------------------------------------------------------
    // Factory methods for common test signals
    //------------------------------------------------------------------

    // Single CW tone (e.g., for basic mixer testing)
    static ToneGenerator cw(double frequency_hz, double amplitudeV = 0.1) {
        return ToneGenerator(frequency_hz, amplitudeV);
    }

    // Two-tone for IMD testing (equal amplitude)
    // Default spacing is 1kHz for typical IMD measurements
    static ToneGenerator twoTone(double center_hz, double spacing_hz = 1000.0,
                                  double amplitudeV = 0.1) {
        ToneGenerator gen;
        gen.addTone(center_hz - spacing_hz / 2.0, amplitudeV);
        gen.addTone(center_hz + spacing_hz / 2.0, amplitudeV);
        return gen;
    }

    // AM modulated carrier
    // carrier(t) = A * (1 + m*sin(2*pi*f_mod*t)) * sin(2*pi*f_c*t)
    //            = A*sin(2*pi*f_c*t) + (A*m/2)*sin(2*pi*(f_c+f_mod)*t)
    //                                - (A*m/2)*sin(2*pi*(f_c-f_mod)*t)
    static ToneGenerator amSignal(double carrierHz, double mod_freq_hz,
                                   double carrier_amp_v = 0.1, double mod_depth = 0.5) {
        ToneGenerator gen;
        gen.addTone(carrierHz, carrier_amp_v, 0.0);
        gen.addTone(carrierHz + mod_freq_hz, carrier_amp_v * mod_depth / 2.0, 90.0);
        gen.addTone(carrierHz - mod_freq_hz, carrier_amp_v * mod_depth / 2.0, -90.0);
        return gen;
    }

    // SSB signal (USB - upper sideband)
    static ToneGenerator usbSignal(double carrierHz, double audio_hz = 1000.0,
                                    double amplitudeV = 0.1) {
        return ToneGenerator(carrierHz + audio_hz, amplitudeV);
    }

    // SSB signal (LSB - lower sideband)
    static ToneGenerator lsbSignal(double carrierHz, double audio_hz = 1000.0,
                                    double amplitudeV = 0.1) {
        return ToneGenerator(carrierHz - audio_hz, amplitudeV);
    }

private:
    std::vector<ToneParams> tones_;
};

} // namespace nexrx
