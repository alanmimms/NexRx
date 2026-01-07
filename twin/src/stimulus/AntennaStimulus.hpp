// NexRx Digital Twin - Antenna Stimulus Interface
//
// Abstract interface for RF signal injection into the simulation.
// Implementations provide different signal types: tones, noise, I/Q playback.
//
// Copyright 2026 NexRx Project - MIT License

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace nexrx {

//======================================================================
// Antenna Stimulus Base Class
//
// Provides RF voltage samples at antenna input.
// Time is in seconds for compatibility with Xyce.
//======================================================================
class AntennaStimulus {
public:
    virtual ~AntennaStimulus() = default;

    // Get voltage at specified simulation time (seconds)
    // Returns voltage in volts (typically -1 to +1 range for normalized signals)
    [[nodiscard]] virtual double getSample(double time_s) const = 0;

    // Get description of stimulus for logging
    [[nodiscard]] virtual std::string description() const = 0;

    // Reset internal state (for looping playback, etc.)
    virtual void reset() {}

    // Check if stimulus has more samples (for finite-length sources)
    [[nodiscard]] virtual bool hasMore(double time_s) const {
        (void)time_s;
        return true;
    }

    // Get baseband I/Q samples given LO frequency (for functional simulation)
    // Default returns zero - override for carrier-based signals
    // This avoids RF aliasing when sampling at audio rates
    virtual void getBasebandIQ(double time_s, double lo_freq_hz,
                               double& out_i, double& out_q) const {
        (void)time_s; (void)lo_freq_hz;
        out_i = out_q = 0.0;
    }

    // Get carrier frequency (0 if no carrier, e.g., noise)
    [[nodiscard]] virtual double carrierFrequency() const { return 0.0; }

    // Get current envelope/amplitude (for keyed signals like CW)
    [[nodiscard]] virtual double getEnvelope(double time_s) const {
        (void)time_s;
        return 1.0;  // Default: constant envelope
    }
};

//======================================================================
// Composite Stimulus - combines multiple sources
//======================================================================
class CompositeStimulus : public AntennaStimulus {
public:
    void addSource(std::shared_ptr<AntennaStimulus> source) {
        sources_.push_back(std::move(source));
    }

    [[nodiscard]] double getSample(double time_s) const override {
        double sum = 0.0;
        for (const auto& src : sources_) {
            sum += src->getSample(time_s);
        }
        return sum;
    }

    [[nodiscard]] std::string description() const override {
        std::string desc = "Composite[";
        for (size_t i = 0; i < sources_.size(); ++i) {
            if (i > 0) desc += ", ";
            desc += sources_[i]->description();
        }
        desc += "]";
        return desc;
    }

    void reset() override {
        for (auto& src : sources_) {
            src->reset();
        }
    }

    [[nodiscard]] bool hasMore(double time_s) const override {
        for (const auto& src : sources_) {
            if (!src->hasMore(time_s)) return false;
        }
        return true;
    }

private:
    std::vector<std::shared_ptr<AntennaStimulus>> sources_;
};

} // namespace nexrx
