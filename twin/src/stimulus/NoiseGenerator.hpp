// NexRx Digital Twin - Noise Generator
//
// Generates Gaussian white noise for simulating thermal noise floor.
// Configurable noise power referenced to bandwidth.
//
// Copyright 2026 NexRx Project - MIT License

#pragma once

#include "AntennaStimulus.hpp"

#include <cmath>
#include <random>
#include <string>
#include <sstream>
#include <iomanip>

namespace nexrx {

//======================================================================
// Noise Generator
//
// Generates band-limited Gaussian white noise.
//
// Thermal noise power: P = kTB
//   k = 1.38e-23 J/K (Boltzmann)
//   T = temperature in Kelvin
//   B = bandwidth in Hz
//
// Into 50 ohms: V_rms = sqrt(P * 50) = sqrt(kTB * 50)
// At 290K, 1Hz BW: V_rms = 0.9 nV
//
// For simulation, we specify RMS voltage directly or derive from
// noise figure and bandwidth.
//======================================================================
class NoiseGenerator : public AntennaStimulus {
public:
    // Direct construction with RMS voltage
    explicit NoiseGenerator(double rms_voltage_v, uint64_t seed = 0)
        : rms_v_(rms_voltage_v)
        , gen_(seed == 0 ? std::random_device{}() : seed)
        , dist_(0.0, 1.0)
    {}

    [[nodiscard]] double getSample(double timeS) const override {
        (void)timeS;  // White noise is time-independent
        return rms_v_ * dist_(gen_);
    }

    // RF I/Q for noise: independent Gaussian samples on I and Q
    // Noise is broadband - no carrier, mixing doesn't change its character
    void getRfIQ(double timeS, double& out_i, double& out_q) const override {
        (void)timeS;
        // Noise is broadband - equal contribution to I and Q
        // Divide by sqrt(2) to maintain total power
        double scale = rms_v_ / std::sqrt(2.0);
        out_i = scale * dist_(gen_);
        out_q = scale * dist_(gen_);
    }

    [[nodiscard]] std::string description() const override {
        std::ostringstream oss;
        oss << std::scientific << std::setprecision(2);
        oss << "Noise(" << rms_v_ * 1e6 << "uV rms)";
        return oss.str();
    }

    void reset() override {
        // Optionally reseed for reproducibility
    }

    // Get configured RMS voltage
    [[nodiscard]] double rmsVoltage() const { return rms_v_; }

    // Set new RMS voltage
    void setRmsVoltage(double rms_v) { rms_v_ = rms_v; }

    // Reseed the random generator
    void seed(uint64_t s) {
        gen_.seed(s);
    }

    //------------------------------------------------------------------
    // Factory methods
    //------------------------------------------------------------------

    // Thermal noise at given temperature and bandwidth into impedance
    // V_rms = sqrt(4 * k * T * B * R) for Johnson noise
    static NoiseGenerator thermal(double temp_k, double bandwidthHz,
                                   double impedance_ohm = 50.0, uint64_t seed = 0) {
        constexpr double kBoltzmann = 1.380649e-23;  // J/K
        double power_w = kBoltzmann * temp_k * bandwidthHz;
        double rms_v = std::sqrt(4.0 * power_w * impedance_ohm);
        return NoiseGenerator(rms_v, seed);
    }

    // Noise from noise figure specification
    // NF = 10*log10(SNR_in / SNR_out) = 10*log10(1 + Te/T0)
    // Te = T0 * (10^(NF/10) - 1)
    // Total noise = thermal + added noise
    static NoiseGenerator fromNoiseFigure(double noise_figure_db, double bandwidthHz,
                                           double impedance_ohm = 50.0,
                                           double ref_temp_k = 290.0,
                                           uint64_t seed = 0) {
        constexpr double kBoltzmann = 1.380649e-23;

        // Equivalent noise temperature
        double nf_linear = std::pow(10.0, noise_figure_db / 10.0);
        double te = ref_temp_k * (nf_linear - 1.0);
        double total_temp = ref_temp_k + te;

        double power_w = kBoltzmann * total_temp * bandwidthHz;
        double rms_v = std::sqrt(4.0 * power_w * impedance_ohm);
        return NoiseGenerator(rms_v, seed);
    }

    // Atmospheric noise at HF (simplified model)
    // Fa = atmospheric noise figure in dB above kT0B
    // Typical values: 50-80 dB at 1 MHz, 20-40 dB at 30 MHz
    static NoiseGenerator atmospheric(double freqHz, double bandwidthHz,
                                       double impedance_ohm = 50.0,
                                       uint64_t seed = 0) {
        // Simplified ITU-R P.372 model for quiet rural location
        // Fa = 55 - 28*log10(f_MHz) for f > 2 MHz
        double f_mhz = freqHz / 1e6;
        double fa_db;
        if (f_mhz < 2.0) {
            fa_db = 76.0 - 20.0 * std::log10(f_mhz);
        } else {
            fa_db = 55.0 - 28.0 * std::log10(f_mhz);
        }

        // Convert to equivalent noise temperature
        constexpr double T0 = 290.0;
        constexpr double kBoltzmann = 1.380649e-23;
        double ta = T0 * std::pow(10.0, fa_db / 10.0);

        double power_w = kBoltzmann * ta * bandwidthHz;
        double rms_v = std::sqrt(4.0 * power_w * impedance_ohm);
        return NoiseGenerator(rms_v, seed);
    }

    // S-meter reference levels (approximate voltage at 50 ohm antenna)
    // S9 = 50 uV (-73 dBm)
    // Each S-unit = 6 dB
    static NoiseGenerator sLevel(int s_units, uint64_t seed = 0) {
        // S9 = 50 uV into 50 ohms
        constexpr double s9_uv = 50.0;
        double level_uv = s9_uv * std::pow(10.0, (s_units - 9) * 6.0 / 20.0);
        return NoiseGenerator(level_uv * 1e-6, seed);
    }

private:
    double rms_v_;
    mutable std::mt19937_64 gen_;
    mutable std::normal_distribution<double> dist_;
};

} // namespace nexrx
