// NexRx Digital Twin - Frequency Sweep Generator
//
// Generates a continuous tone that sweeps linearly between frequencies.
// Useful for measuring receiver passband response.
//
// Copyright 2026 NexRx Project - MIT License

#pragma once

#include "AntennaStimulus.hpp"

#include <cmath>
#include <string>

namespace nexrx {

//======================================================================
// Sweep Generator
//
// Generates a continuous RF signal that sweeps smoothly between
// two frequencies at a configurable rate. Uses phase-continuous
// frequency modulation for clean sweeps without discontinuities.
//
// Sweep modes:
// - Linear: frequency changes at constant rate (Hz/s)
// - Logarithmic: frequency changes at constant ratio rate (decades/s)
//
// Repeat modes:
// - OneShot: sweep once then stop
// - Repeat: sweep repeatedly in same direction
// - PingPong: sweep back and forth
//======================================================================
class SweepGenerator : public AntennaStimulus {
public:
    enum class SweepMode { Linear, Logarithmic };
    enum class RepeatMode { OneShot, Repeat, PingPong };

    // Constructor
    // start_hz: starting frequency
    // end_hz: ending frequency
    // amplitude_v: signal amplitude in volts
    // sweep_rate: Hz/s for linear, decades/s for log
    SweepGenerator(double start_hz, double end_hz,
                   double amplitude_v, double sweep_rate_hz_per_s);

    //------------------------------------------------------------------
    // Configuration
    //------------------------------------------------------------------

    void setFrequencyRange(double start_hz, double end_hz);
    void setSweepRate(double rate_hz_per_s);
    void setAmplitude(double amplitude_v) { amplitude_v_ = amplitude_v; }
    void setSweepMode(SweepMode mode) { sweepMode_ = mode; }
    void setRepeatMode(RepeatMode mode) { repeatMode_ = mode; }

    // Set envelope rise/fall time for start/stop (default 5ms)
    void setEnvelopeTime(double ms) { envelopeTime_ms_ = ms; }

    //------------------------------------------------------------------
    // AntennaStimulus interface
    //------------------------------------------------------------------

    [[nodiscard]] double getSample(double time_s) const override;
    [[nodiscard]] std::string description() const override;
    void reset() override;
    [[nodiscard]] bool hasMore(double time_s) const override;

    // Analytic RF signal
    void getRfIQ(double time_s, double& out_i, double& out_q) const override;
    [[nodiscard]] double carrierFrequency() const override { return (startFreq_hz_ + endFreq_hz_) / 2.0; }

    //------------------------------------------------------------------
    // Accessors
    //------------------------------------------------------------------

    [[nodiscard]] double startFrequency() const { return startFreq_hz_; }
    [[nodiscard]] double endFrequency() const { return endFreq_hz_; }
    [[nodiscard]] double sweepRate() const { return sweepRate_hz_per_s_; }
    [[nodiscard]] double amplitude() const { return amplitude_v_; }
    [[nodiscard]] double sweepDuration() const { return sweepDuration_s_; }

    // Get instantaneous frequency at given time
    [[nodiscard]] double frequencyAt(double time_s) const;

    //------------------------------------------------------------------
    // Factory methods for common configurations
    //------------------------------------------------------------------

    // Create a linear sweep from start to end at given rate
    static SweepGenerator linearSweep(double start_hz, double end_hz,
                                       double amplitude_v, double rate_hz_per_s);

    // Create a sweep that covers a range in a specific duration
    static SweepGenerator timedSweep(double start_hz, double end_hz,
                                      double amplitude_v, double duration_s);

    // Create an HF band sweep (1-30 MHz) for receiver characterization
    static SweepGenerator hfBandSweep(double amplitude_v, double duration_s);

private:
    // Calculate phase-continuous instantaneous phase
    double getPhase(double time_s) const;

    // Get phase accumulated over duration (for forward sweep)
    double getPhaseForDuration(double t) const;

    // Get phase for reverse sweep segment
    double getPhaseForReverse(double t) const;

    // Get envelope at time (for smooth start/stop)
    double getEnvelope(double time_s) const;

    double startFreq_hz_;
    double endFreq_hz_;
    double amplitude_v_;
    double sweepRate_hz_per_s_;

    SweepMode sweepMode_ = SweepMode::Linear;
    RepeatMode repeatMode_ = RepeatMode::Repeat;

    double envelopeTime_ms_ = 5.0;

    // Derived values
    double sweepDuration_s_;
    double freqRange_hz_;
};

} // namespace nexrx
