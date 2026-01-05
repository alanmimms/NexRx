// NexRx Digital Twin - ADC Sampler Implementation
//
// Copyright 2026 NexRx Project - MIT License

#include "AdcSampler.hpp"

#include <algorithm>
#include <cmath>

namespace nexrx {

AdcSampler::AdcSampler(NexRx::Twin::XyceWrapper& xyce, const AdcConfig& config)
    : xyce_(xyce)
    , config_(config)
{
}

double AdcSampler::nextSampleTime() const {
    return static_cast<double>(nextSampleTimeNs_) * 1e-9;
}

uint64_t AdcSampler::nextSampleTimeNs() const {
    return nextSampleTimeNs_;
}

bool AdcSampler::isSampleDue() const {
    auto currentTime = xyce_.getCurrentTime();
    if (!currentTime) {
        return false;
    }

    double currentTimeNs = *currentTime * 1e9;
    return currentTimeNs >= static_cast<double>(nextSampleTimeNs_);
}

int32_t AdcSampler::voltageToAdc(double voltage, double vcm) {
    // Remove DC bias (common-mode voltage)
    double vDiff = voltage - vcm;

    // Scale to ADC range: +/- 1.65V -> +/- 2^23
    double scaled = vDiff / AdcConfig::FULL_SCALE * static_cast<double>(AdcConfig::ADC_MAX);

    // Clamp to valid range
    scaled = std::clamp(scaled,
                        static_cast<double>(AdcConfig::ADC_MIN),
                        static_cast<double>(AdcConfig::ADC_MAX));

    return static_cast<int32_t>(std::round(scaled));
}

std::optional<IQSample> AdcSampler::sampleChannel(size_t channelIndex) {
    if (channelIndex >= 3) {
        return std::nullopt;
    }

    // Get I and Q voltages for this channel
    auto vI = xyce_.getNodeVoltage(config_.i_nodes[channelIndex]);
    auto vQ = xyce_.getNodeVoltage(config_.q_nodes[channelIndex]);

    if (!vI || !vQ) {
        ++errorCount_;
        return std::nullopt;
    }

    // Convert to ADC values
    IQSample sample;
    sample.i = voltageToAdc(*vI);
    sample.q = voltageToAdc(*vQ);

    return sample;
}

std::optional<IQFrame> AdcSampler::sample() {
    // Check if Xyce has reached the sample time
    auto currentTime = xyce_.getCurrentTime();
    if (!currentTime) {
        return std::nullopt;
    }

    double currentTimeNs = *currentTime * 1e9;

    // Check for missed samples
    while (static_cast<double>(nextSampleTimeNs_) + AdcConfig::SAMPLE_PERIOD_NS < currentTimeNs) {
        // We missed a sample
        ++missedSamples_;
        nextSampleTimeNs_ += AdcConfig::SAMPLE_PERIOD_NS;
        ++sequence_;
    }

    if (currentTimeNs < static_cast<double>(nextSampleTimeNs_)) {
        // Not time for a sample yet
        return std::nullopt;
    }

    // Create frame
    IQFrame frame;
    frame.timestamp_ns = nextSampleTimeNs_;
    frame.sequence = sequence_;
    frame.flags = 0;

    // Sample all three QSD channels
    bool allValid = true;
    for (size_t i = 0; i < 3; ++i) {
        auto sample = sampleChannel(i);
        if (sample) {
            frame.qsd[i] = *sample;
        } else {
            frame.qsd[i] = IQSample{0, 0};
            allValid = false;
        }
    }

    // Update timing for next sample
    nextSampleTimeNs_ += AdcConfig::SAMPLE_PERIOD_NS;
    ++sampleCount_;
    ++sequence_;

    if (!allValid) {
        frame.flags |= 0x01;  // Mark frame as having errors
    }

    return frame;
}

bool AdcSampler::sampleWithCallback(const FrameCallback& callback) {
    auto frame = sample();
    if (frame && callback) {
        callback(*frame);
        return true;
    }
    return false;
}

void AdcSampler::reset() {
    nextSampleTimeNs_ = 0;
    sampleCount_ = 0;
    sequence_ = 0;
    missedSamples_ = 0;
    errorCount_ = 0;
}

} // namespace nexrx
