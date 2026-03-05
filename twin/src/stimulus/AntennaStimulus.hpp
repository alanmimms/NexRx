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
    [[nodiscard]] virtual double getSample(double timeS) const = 0;

    // Get description of stimulus for logging
    [[nodiscard]] virtual std::string description() const = 0;

    // Reset internal state (for looping playback, etc.)
    virtual void reset() {}

    // Check if stimulus has more samples (for finite-length sources)
    [[nodiscard]] virtual bool hasMore(double timeS) const {
        (void)timeS;
        return true;
    }

    // Get analytic RF signal (complex envelope at carrier frequency)
    // Returns I/Q components of: signal(t) = I*cos(2π*fc*t) - Q*sin(2π*fc*t)
    // This represents the RF signal WITHOUT any knowledge of receiver LO.
    // The QSD simulation layer will mix this with LO to produce baseband.
    // Default returns zero - override for carrier-based signals.
    virtual void getRfIQ(double timeS, double& out_i, double& out_q) const {
        (void)timeS;
        out_i = out_q = 0.0;
    }

    // Get carrier frequency (0 if no carrier, e.g., noise)
    [[nodiscard]] virtual double carrierFrequency() const { return 0.0; }

    // Get current envelope/amplitude (for keyed signals like CW)
    [[nodiscard]] virtual double getEnvelope(double timeS) const {
        (void)timeS;
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

    [[nodiscard]] double getSample(double timeS) const override {
        double sum = 0.0;
        for (const auto& src : sources_) {
            sum += src->getSample(timeS);
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

    [[nodiscard]] bool hasMore(double timeS) const override {
        for (const auto& src : sources_) {
            if (!src->hasMore(timeS)) return false;
        }
        return true;
    }

private:
    std::vector<std::shared_ptr<AntennaStimulus>> sources_;
};

} // namespace nexrx
