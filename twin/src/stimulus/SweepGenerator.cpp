// NexRx Digital Twin - Frequency Sweep Generator Implementation
//
// Copyright 2026 NexRx Project - MIT License

#include "SweepGenerator.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace nexrx {

SweepGenerator::SweepGenerator(double start_hz, double end_hz,
                               double amplitude_v, double sweep_rate_hz_per_s)
    : startFreq_hz_(start_hz)
    , endFreq_hz_(end_hz)
    , amplitude_v_(amplitude_v)
    , sweepRate_hz_per_s_(std::abs(sweep_rate_hz_per_s))
{
    freqRange_hz_ = end_hz - start_hz;
    sweepDuration_s_ = std::abs(freqRange_hz_) / sweepRate_hz_per_s_;
}

void SweepGenerator::setFrequencyRange(double start_hz, double end_hz) {
    startFreq_hz_ = start_hz;
    endFreq_hz_ = end_hz;
    freqRange_hz_ = end_hz - start_hz;
    sweepDuration_s_ = std::abs(freqRange_hz_) / sweepRate_hz_per_s_;
}

void SweepGenerator::setSweepRate(double rate_hz_per_s) {
    sweepRate_hz_per_s_ = std::abs(rate_hz_per_s);
    sweepDuration_s_ = std::abs(freqRange_hz_) / sweepRate_hz_per_s_;
}

double SweepGenerator::frequencyAt(double time_s) const {
    double effectiveTime = time_s;
    bool reverse = false;

    switch (repeatMode_) {
        case RepeatMode::OneShot:
            effectiveTime = std::clamp(time_s, 0.0, sweepDuration_s_);
            break;

        case RepeatMode::Repeat:
            effectiveTime = std::fmod(time_s, sweepDuration_s_);
            if (effectiveTime < 0) effectiveTime += sweepDuration_s_;
            break;

        case RepeatMode::PingPong: {
            double cycleDuration = 2.0 * sweepDuration_s_;
            effectiveTime = std::fmod(time_s, cycleDuration);
            if (effectiveTime < 0) effectiveTime += cycleDuration;
            if (effectiveTime > sweepDuration_s_) {
                effectiveTime = cycleDuration - effectiveTime;
                reverse = true;
            }
            break;
        }
    }

    double progress = effectiveTime / sweepDuration_s_;

    switch (sweepMode_) {
        case SweepMode::Linear:
            return startFreq_hz_ + progress * freqRange_hz_;

        case SweepMode::Logarithmic: {
            // Log sweep: f(t) = f_start * (f_end/f_start)^(t/T)
            double ratio = endFreq_hz_ / startFreq_hz_;
            return startFreq_hz_ * std::pow(ratio, progress);
        }
    }

    return startFreq_hz_;
}

double SweepGenerator::getPhase(double time_s) const {
    // For phase-continuous sweep, we need to integrate frequency over time
    // For linear sweep: f(t) = f0 + k*t where k = sweep_rate
    // Phase: φ(t) = ∫ 2π f(τ) dτ = 2π (f0*t + k*t²/2)

    double effectiveTime = time_s;
    double phaseOffset = 0.0;
    int cycleCount = 0;

    switch (repeatMode_) {
        case RepeatMode::OneShot:
            effectiveTime = std::min(time_s, sweepDuration_s_);
            break;

        case RepeatMode::Repeat:
            cycleCount = static_cast<int>(time_s / sweepDuration_s_);
            effectiveTime = time_s - cycleCount * sweepDuration_s_;
            // Add phase from completed cycles
            phaseOffset = cycleCount * getPhaseForDuration(sweepDuration_s_);
            break;

        case RepeatMode::PingPong: {
            double cycleDuration = 2.0 * sweepDuration_s_;
            cycleCount = static_cast<int>(time_s / cycleDuration);
            double timeInCycle = time_s - cycleCount * cycleDuration;

            // Phase accumulated in one full ping-pong cycle
            double fullCyclePhase = 2.0 * getPhaseForDuration(sweepDuration_s_);
            phaseOffset = cycleCount * fullCyclePhase;

            if (timeInCycle <= sweepDuration_s_) {
                effectiveTime = timeInCycle;
            } else {
                // Reverse sweep
                double reverseTime = timeInCycle - sweepDuration_s_;
                phaseOffset += getPhaseForDuration(sweepDuration_s_);
                // For reverse, we integrate from end frequency going backwards
                return phaseOffset + getPhaseForReverse(reverseTime);
            }
            break;
        }
    }

    return phaseOffset + getPhaseForDuration(effectiveTime);
}

double SweepGenerator::getPhaseForDuration(double t) const {
    switch (sweepMode_) {
        case SweepMode::Linear: {
            // f(τ) = f_start + rate * τ
            // φ(t) = 2π ∫₀ᵗ (f_start + rate*τ) dτ
            //      = 2π (f_start*t + rate*t²/2)
            double rate = freqRange_hz_ / sweepDuration_s_;
            return 2.0 * M_PI * (startFreq_hz_ * t + rate * t * t / 2.0);
        }

        case SweepMode::Logarithmic: {
            // f(τ) = f_start * exp(k*τ) where k = ln(f_end/f_start)/T
            // φ(t) = 2π ∫₀ᵗ f_start * exp(k*τ) dτ
            //      = 2π * f_start * (exp(k*t) - 1) / k
            double k = std::log(endFreq_hz_ / startFreq_hz_) / sweepDuration_s_;
            if (std::abs(k) < 1e-10) {
                // Nearly constant frequency
                return 2.0 * M_PI * startFreq_hz_ * t;
            }
            return 2.0 * M_PI * startFreq_hz_ * (std::exp(k * t) - 1.0) / k;
        }
    }
    return 0.0;
}

double SweepGenerator::getPhaseForReverse(double t) const {
    // Reverse sweep: frequency goes from endFreq back to startFreq
    switch (sweepMode_) {
        case SweepMode::Linear: {
            double rate = -freqRange_hz_ / sweepDuration_s_;  // Negative rate
            return 2.0 * M_PI * (endFreq_hz_ * t + rate * t * t / 2.0);
        }

        case SweepMode::Logarithmic: {
            double k = std::log(startFreq_hz_ / endFreq_hz_) / sweepDuration_s_;
            if (std::abs(k) < 1e-10) {
                return 2.0 * M_PI * endFreq_hz_ * t;
            }
            return 2.0 * M_PI * endFreq_hz_ * (std::exp(k * t) - 1.0) / k;
        }
    }
    return 0.0;
}

double SweepGenerator::getEnvelope(double time_s) const {
    double envelopeTime_s = envelopeTime_ms_ / 1000.0;

    // Smooth start
    if (time_s < envelopeTime_s) {
        double t = time_s / envelopeTime_s;
        return 0.5 * (1.0 - std::cos(M_PI * t));  // Raised cosine
    }

    // For one-shot mode, smooth end
    if (repeatMode_ == RepeatMode::OneShot) {
        if (time_s > sweepDuration_s_ - envelopeTime_s) {
            double t = (sweepDuration_s_ - time_s) / envelopeTime_s;
            return 0.5 * (1.0 - std::cos(M_PI * t));
        }
        if (time_s >= sweepDuration_s_) {
            return 0.0;
        }
    }

    return 1.0;
}

double SweepGenerator::getSample(double time_s) const {
    if (time_s < 0) return 0.0;

    // One-shot mode: stop after sweep completes
    if (repeatMode_ == RepeatMode::OneShot && time_s > sweepDuration_s_) {
        return 0.0;
    }

    double phase = getPhase(time_s);
    double envelope = getEnvelope(time_s);

    return amplitude_v_ * envelope * std::sin(phase);
}

std::string SweepGenerator::description() const {
    std::ostringstream oss;
    oss << "Sweep[" << startFreq_hz_ / 1e6 << "-"
        << endFreq_hz_ / 1e6 << " MHz, "
        << sweepRate_hz_per_s_ / 1e6 << " MHz/s";
    if (sweepMode_ == SweepMode::Logarithmic) {
        oss << ", log";
    }
    oss << "]";
    return oss.str();
}

void SweepGenerator::reset() {
    // Nothing to reset - time-indexed
}

bool SweepGenerator::hasMore(double time_s) const {
    if (repeatMode_ == RepeatMode::OneShot) {
        return time_s < sweepDuration_s_;
    }
    return true;
}

void SweepGenerator::getRfIQ(double time_s, double& out_i, double& out_q) const {
    if (time_s < 0) {
        out_i = out_q = 0.0;
        return;
    }

    if (repeatMode_ == RepeatMode::OneShot && time_s > sweepDuration_s_) {
        out_i = out_q = 0.0;
        return;
    }

    double phase = getPhase(time_s);
    double envelope = getEnvelope(time_s) * amplitude_v_;

    out_i = envelope * std::cos(phase);
    out_q = envelope * std::sin(phase);
}

// Factory methods

SweepGenerator SweepGenerator::linearSweep(double start_hz, double end_hz,
                                            double amplitude_v, double rate_hz_per_s) {
    return SweepGenerator(start_hz, end_hz, amplitude_v, rate_hz_per_s);
}

SweepGenerator SweepGenerator::timedSweep(double start_hz, double end_hz,
                                           double amplitude_v, double duration_s) {
    double rate = std::abs(end_hz - start_hz) / duration_s;
    return SweepGenerator(start_hz, end_hz, amplitude_v, rate);
}

SweepGenerator SweepGenerator::hfBandSweep(double amplitude_v, double duration_s) {
    // 1 MHz to 30 MHz sweep
    return timedSweep(1.0e6, 30.0e6, amplitude_v, duration_s);
}

} // namespace nexrx
