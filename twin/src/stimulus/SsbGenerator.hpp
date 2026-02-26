// NexRx Digital Twin - SSB Signal Generator
//
// Generates Single Sideband (USB/LSB) signals with multi-tone
// or voice modulation using Hilbert transform.
//
// Copyright 2026 NexRx Project - MIT License

#pragma once

#include "AntennaStimulus.hpp"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace nexrx {

// Forward declaration for TTS integration
class TtsEngine;

//======================================================================
// SSB Generator
//
// Generates USB or LSB signals from audio input (tones or voice).
// Uses phasing method: USB = I*cos - Q*sin, LSB = I*cos + Q*sin
// where Q is the Hilbert transform of I.
//======================================================================
class SsbGenerator : public AntennaStimulus {
public:
    enum class Mode { USB, LSB };

    // Constructor
    // carrier_hz: suppressed carrier frequency
    // amplitude_v: peak RF amplitude
    // mode: USB or LSB
    SsbGenerator(double carrier_hz, double amplitude_v, Mode mode);

    ~SsbGenerator() override;

    //------------------------------------------------------------------
    // Configuration
    //------------------------------------------------------------------

    void setCarrier(double freq_hz) { carrier_hz_ = freq_hz; }
    void setAmplitude(double amplitude_v) { amplitude_v_ = amplitude_v; }
    void setMode(Mode mode) { mode_ = mode; }

    // Set audio as multiple tones (frequencies in Hz, typically 300-3000 Hz)
    void setTones(const std::vector<double>& audio_freqs_hz);

    // Set audio from espeak-ng TTS engine
    void setVoice(std::shared_ptr<TtsEngine> tts, bool repeat = true);

    // Set audio from raw samples (sample rate must be specified)
    void setAudioSamples(std::vector<float> samples, double sample_rate, bool repeat = true);

    //------------------------------------------------------------------
    // AntennaStimulus interface
    //------------------------------------------------------------------

    [[nodiscard]] double getSample(double time_s) const override;
    [[nodiscard]] std::string description() const override;
    void reset() override;

    // Analytic RF signal (complex envelope at carrier)
    void getRfIQ(double time_s, double& out_i, double& out_q) const override;
    [[nodiscard]] double carrierFrequency() const override { return carrier_hz_; }

    //------------------------------------------------------------------
    // Accessors
    //------------------------------------------------------------------

    [[nodiscard]] double carrier() const { return carrier_hz_; }
    [[nodiscard]] double amplitude() const { return amplitude_v_; }
    [[nodiscard]] Mode mode() const { return mode_; }

private:
    // Audio source types
    enum class AudioSource { None, Tones, Voice, Samples };

    // Get audio I/Q at given time (Q is Hilbert of I)
    void getAudioIQ(double time_s, double& i, double& q) const;

    // Hilbert filter for arbitrary audio
    double hilbertFilter(double time_s) const;

    double carrier_hz_;
    double amplitude_v_;
    Mode mode_;

    AudioSource audioSource_ = AudioSource::None;

    // Tone mode
    struct ToneInfo {
        double freq_hz;
        double amplitude;  // Normalized (sum = 1)
    };
    std::vector<ToneInfo> tones_;

    // Voice mode (espeak-ng)
    std::shared_ptr<TtsEngine> tts_;
    bool voiceRepeat_ = true;

    // Sample mode
    std::vector<float> audioSamples_;
    std::vector<float> audioSamplesQ_;  // Pre-computed Hilbert transform
    double audioSampleRate_ = 0.0;
    bool samplesRepeat_ = true;

    // Hilbert filter coefficients (FIR approximation)
    static constexpr size_t HILBERT_TAPS = 65;
    std::vector<double> hilbertCoeffs_;
    mutable std::vector<double> hilbertHistory_;
    mutable size_t hilbertIndex_ = 0;
    mutable double lastSampleTime_ = -1.0;

    mutable double carrierPhase_ = 0.0;
    mutable double lastTime_ = -1.0;

    void initHilbertFilter();
    void precomputeHilbert();  // Pre-compute Q channel for samples
    void resampleToInternalRate(const std::vector<float>& input, double inputRate);
    void applyLoopCrossfade();  // Smooth loop boundary to prevent transients
};

} // namespace nexrx
