// NexRx Digital Twin - AM Signal Generator
//
// Generates Amplitude Modulated (AM) signals with multi-tone
// or voice modulation.
//
// Copyright 2026 NexRx Project - MIT License

#pragma once

#include "AntennaStimulus.hpp"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace nexrx {

//======================================================================
// AM Generator
//
// Generates AM signals: RF = Ac * (1 + m * modulation) * cos(wc * t)
//======================================================================
class AmGenerator : public AntennaStimulus {
public:
    // Constructor
    // carrier_hz: carrier frequency
    // amplitude_v: peak RF amplitude (unmodulated carrier)
    AmGenerator(double carrier_hz, double amplitude_v);

    ~AmGenerator() override;

    //------------------------------------------------------------------
    // Configuration
    //------------------------------------------------------------------

    void setCarrier(double freq_hz) { carrier_hz_ = freq_hz; }
    void setAmplitude(double amplitude_v) { amplitude_v_ = amplitude_v; }
    void setModulationIndex(double m) { modIndex_ = m; }

    // Set modulation as multiple tones (frequencies in Hz)
    void setTones(const std::vector<double>& audio_freqs_hz);

    // Set modulation from raw samples (sample rate must be specified)
    void setAudioSamples(std::vector<float> samples, double sample_rate, bool repeat = true);

    //------------------------------------------------------------------
    // AntennaStimulus interface
    //------------------------------------------------------------------

    [[nodiscard]] double getSample(double time_s) const override;
    [[nodiscard]] std::string description() const override;
    void reset() override;

    // Analytic RF signal
    void getRfIQ(double time_s, double& out_i, double& out_q) const override;
    [[nodiscard]] double carrierFrequency() const override { return carrier_hz_; }

private:
    // Audio source types
    enum class AudioSource { None, Tones, Samples };

    // Get modulation signal at given time (range -1 to 1)
    double getModulation(double time_s) const;

    double carrier_hz_;
    double amplitude_v_;
    double modIndex_ = 0.8; // Default 80% modulation

    AudioSource audioSource_ = AudioSource::None;

    // Tone mode
    struct ToneInfo {
        double freq_hz;
        double amplitude;
    };
    std::vector<ToneInfo> tones_;

    // Sample mode
    std::vector<float> audioSamples_;
    double audioSampleRate_ = 0.0;
    bool samplesRepeat_ = true;

    mutable double carrierPhase_ = 0.0;
    mutable double lastTime_ = -1.0;
};

} // namespace nexrx
