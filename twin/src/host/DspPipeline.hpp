// NexRx Digital Twin - DSP Pipeline
//
// Basic signal processing for triple-QSD I/Q data.
// Combines signals from three QSDs for image rejection.
//
// Copyright 2026 NexRx Project - MIT License

#pragma once

#include "transport/IQFrame.hpp"

#include <array>
#include <cmath>
#include <complex>
#include <deque>
#include <functional>
#include <vector>

namespace nexrx {

//======================================================================
// Complex sample type
//======================================================================
using Complex = std::complex<double>;

//======================================================================
// DSP Pipeline Configuration
//======================================================================
struct DspConfig {
    // QSD weighting for combining (default: equal weight)
    std::array<double, 3> qsdWeights = {1.0, 1.0, 1.0};

    // Enable QSD2 (sextature) for enhanced image rejection
    bool useSextature = true;

    // DC removal filter time constant (samples)
    size_t dcFilterLength = 96;  // ~1ms at 96kHz

    // AGC parameters
    bool enableAgc = false;
    double agcTargetLevel = 0.5;
    double agcAttackRate = 0.01;
    double agcDecayRate = 0.001;
};

//======================================================================
// Signal Statistics
//======================================================================
struct SignalStats {
    double rmsLevel = 0;        // RMS signal level
    double peakLevel = 0;       // Peak level
    double dcOffset_i = 0;      // DC offset on I
    double dcOffset_q = 0;      // DC offset on Q
    double snrEstimate = 0;     // Estimated SNR (dB)
    double imageRejection = 0;  // Image rejection ratio (dB)

    void reset() {
        rmsLevel = peakLevel = dcOffset_i = dcOffset_q = 0;
        snrEstimate = imageRejection = 0;
    }
};

//======================================================================
// DSP Pipeline
//
// Processes I/Q frames from the triple-QSD receiver:
// 1. Combines signals from QSD0, QSD1, QSD2
// 2. Applies DC removal
// 3. Optionally applies AGC
// 4. Computes signal statistics
//======================================================================
class DspPipeline {
public:
    using OutputCallback = std::function<void(const Complex&, uint64_t timestamp)>;

    DspPipeline() = default;

    // Configure the pipeline
    void configure(const DspConfig& config);

    // Process a single I/Q frame
    // Returns combined complex output sample
    Complex process(const IQFrame& frame);

    // Process batch of frames
    std::vector<Complex> processBatch(const std::vector<IQFrame>& frames);

    // Set callback for processed samples
    void setOutputCallback(OutputCallback callback) {
        outputCallback_ = std::move(callback);
    }

    // Get current statistics
    [[nodiscard]] const SignalStats& stats() const { return stats_; }

    // Reset pipeline state
    void reset();

    // Get current AGC gain
    [[nodiscard]] double agcGain() const { return agcGain_; }

    //------------------------------------------------------------------
    // Static utility functions
    //------------------------------------------------------------------

    // Convert ADC value to voltage
    static double adcToVoltage(int32_t adc) {
        return adc * (1.65 / 8388607.0);
    }

    // Convert I/Q sample to complex
    static Complex sampleToComplex(const IQSample& s) {
        return Complex(adcToVoltage(s.i), adcToVoltage(s.q));
    }

    // Magnitude in dB
    static double magnitudeDb(const Complex& c) {
        double mag = std::abs(c);
        return (mag > 0) ? 20.0 * std::log10(mag) : -120.0;
    }

    // Phase in degrees
    static double phaseDeg(const Complex& c) {
        return std::arg(c) * 180.0 / M_PI;
    }

private:
    // Combine three QSD outputs
    Complex combineQsd(const IQFrame& frame);

    // Remove DC offset
    Complex removeDc(const Complex& sample);

    // Apply AGC
    Complex applyAgc(const Complex& sample);

    // Update statistics
    void updateStats(const Complex& sample);

    DspConfig config_;
    OutputCallback outputCallback_;
    SignalStats stats_;

    // DC removal state (moving average)
    std::deque<Complex> dcHistory_;
    Complex dcSum_{0, 0};

    // AGC state
    double agcGain_ = 1.0;

    // Statistics accumulators
    double sumMagSq_ = 0;
    double peakMag_ = 0;
    size_t sampleCount_ = 0;
};

//======================================================================
// Simple Spectrum Analyzer
//
// Computes power spectrum from I/Q samples using sliding DFT.
// Lightweight alternative to full FFT for basic visualization.
//======================================================================
class SpectrumAnalyzer {
public:
    explicit SpectrumAnalyzer(size_t numBins = 64);

    // Add sample to analyzer
    void addSample(const Complex& sample);

    // Get current spectrum (power in dB per bin)
    [[nodiscard]] const std::vector<double>& spectrum() const { return spectrum_; }

    // Get bin frequencies relative to sample rate
    // Bin 0 = -Fs/2, Bin N/2 = 0 (DC), Bin N-1 = +Fs/2
    [[nodiscard]] double binFrequency(size_t bin, double sampleRate) const;

    // Get peak bin index
    [[nodiscard]] size_t peakBin() const;

    // Reset analyzer
    void reset();

private:
    void updateSpectrum();

    size_t numBins_;
    std::vector<Complex> buffer_;
    std::vector<double> spectrum_;
    std::vector<Complex> twiddles_;  // Precomputed DFT coefficients
    size_t writePos_ = 0;
    bool bufferFull_ = false;
};

} // namespace nexrx
