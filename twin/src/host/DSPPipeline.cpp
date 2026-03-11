// NexRx Digital Twin - DSP Pipeline Implementation
//
// Copyright 2026 NexRx Project - MIT License

#include "DSPPipeline.hpp"

#include <algorithm>
#include <numeric>

namespace nexrx {

//======================================================================
// DSPPipeline Implementation
//======================================================================

void DSPPipeline::configure(const DSPConfig& config) {
    config_ = config;
    reset();
}

void DSPPipeline::reset() {
    dcHistory_.clear();
    dcSum_ = Complex{0, 0};
    agcGain_ = 1.0;
    sumMagSq_ = 0;
    peakMag_ = 0;
    sampleCount_ = 0;
    stats_.reset();
}

Complex DSPPipeline::process(const IQFrame& frame) {
    // Combine QSD outputs
    Complex combined = combineQsd(frame);

    // Remove DC offset
    Complex dcRemoved = removeDc(combined);

    // Apply AGC if enabled
    Complex output = config_.enableAgc ? applyAgc(dcRemoved) : dcRemoved;

    // Update statistics
    updateStats(output);

    // Call output callback
    if (outputCallback_) {
        outputCallback_(output, frame.timestampNS);
    }

    return output;
}

std::vector<Complex> DSPPipeline::processBatch(const std::vector<IQFrame>& frames) {
    std::vector<Complex> output;
    output.reserve(frames.size());

    for (const auto& frame : frames) {
        output.push_back(process(frame));
    }

    return output;
}

Complex DSPPipeline::combineQsd(const IQFrame& frame) {
    // Convert each QSD output to complex
    Complex q0 = sampleToComplex(frame.qsd[0]);
    Complex q1 = sampleToComplex(frame.qsd[1]);
    Complex q2 = sampleToComplex(frame.qsd[2]);

    // Apply weights
    q0 *= config_.qsdWeights[0];
    q1 *= config_.qsdWeights[1];
    q2 *= config_.qsdWeights[2];

    // Basic combining strategy:
    // QSD0 and QSD1 are standard quadrature (4-phase)
    // QSD2 is sextature (6-phase) for enhanced image rejection

    Complex combined;

    if (config_.useSextature && config_.qsdWeights[2] > 0) {
        // Weighted combination of all three
        // The sextature provides additional image rejection
        // Simple average for now - more sophisticated combining
        // would use phase-aligned weighting
        combined = (q0 + q1 + q2) / 3.0;
    } else {
        // Use only QSD0 and QSD1
        combined = (q0 + q1) / 2.0;
    }

    return combined;
}

Complex DSPPipeline::removeDc(const Complex& sample) {
    // Moving average DC estimation
    dcHistory_.push_back(sample);
    dcSum_ += sample;

    while (dcHistory_.size() > config_.dcFilterLength) {
        dcSum_ -= dcHistory_.front();
        dcHistory_.pop_front();
    }

    if (dcHistory_.empty()) {
        return sample;
    }

    Complex dcEstimate = dcSum_ / static_cast<double>(dcHistory_.size());

    // Update DC offset in stats
    stats_.dcOffset_i = dcEstimate.real();
    stats_.dcOffset_q = dcEstimate.imag();

    return sample - dcEstimate;
}

Complex DSPPipeline::applyAgc(const Complex& sample) {
    double mag = std::abs(sample);

    // Update gain based on level
    if (mag * agcGain_ > config_.agcTargetLevel) {
        // Too loud - reduce gain (fast attack)
        agcGain_ *= (1.0 - config_.agcAttackRate);
    } else {
        // Too quiet - increase gain (slow decay)
        agcGain_ *= (1.0 + config_.agcDecayRate);
    }

    // Clamp gain to reasonable range
    agcGain_ = std::clamp(agcGain_, 0.001, 1000.0);

    return sample * agcGain_;
}

void DSPPipeline::updateStats(const Complex& sample) {
    double mag = std::abs(sample);

    sumMagSq_ += mag * mag;
    peakMag_ = std::max(peakMag_, mag);
    ++sampleCount_;

    // Update stats periodically (every 96 samples = 1ms)
    if (sampleCount_ >= 96) {
        stats_.rmsLevel = std::sqrt(sumMagSq_ / sampleCount_);
        stats_.peakLevel = peakMag_;

        // Reset accumulators
        sumMagSq_ = 0;
        peakMag_ = 0;
        sampleCount_ = 0;
    }
}

//======================================================================
// SpectrumAnalyzer Implementation
//======================================================================

SpectrumAnalyzer::SpectrumAnalyzer(size_t numBins)
    : numBins_(numBins)
    , buffer_(numBins)
    , spectrum_(numBins, -120.0)
{
    // Precompute DFT twiddle factors
    twiddles_.resize(numBins);
    for (size_t k = 0; k < numBins; ++k) {
        double angle = -2.0 * M_PI * k / numBins;
        twiddles_[k] = Complex(std::cos(angle), std::sin(angle));
    }
}

void SpectrumAnalyzer::addSample(const Complex& sample) {
    buffer_[writePos_] = sample;
    writePos_ = (writePos_ + 1) % numBins_;

    if (writePos_ == 0) {
        bufferFull_ = true;
        updateSpectrum();
    }
}

void SpectrumAnalyzer::updateSpectrum() {
    if (!bufferFull_) return;

    // Simple DFT (not FFT - OK for small numBins)
    for (size_t k = 0; k < numBins_; ++k) {
        Complex sum{0, 0};
        Complex tw{1, 0};

        for (size_t n = 0; n < numBins_; ++n) {
            size_t idx = (writePos_ + n) % numBins_;
            sum += buffer_[idx] * tw;
            tw *= twiddles_[k];
        }

        // Power in dB
        double power = std::norm(sum) / (numBins_ * numBins_);
        spectrum_[k] = (power > 0) ? 10.0 * std::log10(power) : -120.0;
    }
}

double SpectrumAnalyzer::binFrequency(size_t bin, double sampleRate) const {
    // FFT-shifted: bin 0 = -Fs/2, bin N/2 = DC, bin N-1 = +Fs/2 - df
    double df = sampleRate / numBins_;
    return (static_cast<double>(bin) - numBins_ / 2.0) * df;
}

size_t SpectrumAnalyzer::peakBin() const {
    auto it = std::max_element(spectrum_.begin(), spectrum_.end());
    return std::distance(spectrum_.begin(), it);
}

void SpectrumAnalyzer::reset() {
    std::fill(buffer_.begin(), buffer_.end(), Complex{0, 0});
    std::fill(spectrum_.begin(), spectrum_.end(), -120.0);
    writePos_ = 0;
    bufferFull_ = false;
}

} // namespace nexrx
