// NexRx Digital Twin - Morse Code Generator
//
// Generates CW (continuous wave) signals from text using ITU Morse code.
// Supports configurable WPM, smooth keying envelope, and repeat mode.
//
// Copyright 2026 NexRx Project - MIT License

#pragma once

#include "AntennaStimulus.hpp"

#include <cmath>
#include <string>
#include <vector>

namespace nexrx {

//======================================================================
// Morse Generator
//
// Converts text to Morse code and generates keyed RF signal.
// Uses PARIS standard timing (50 dit-lengths per word).
//======================================================================
class MorseGenerator : public AntennaStimulus {
public:
    // Constructor
    // freq_hz: carrier frequency in Hz
    // amplitude_v: peak amplitude in volts
    // text: message to send (converted to uppercase)
    // wpm: words per minute (PARIS standard)
    MorseGenerator(double freq_hz, double amplitude_v,
                   const std::string& text, double wpm = 20.0);

    //------------------------------------------------------------------
    // Configuration
    //------------------------------------------------------------------

    void setText(const std::string& text);
    void setWpm(double wpm);
    void setFrequency(double freq_hz) { freq_hz_ = freq_hz; }
    void setAmplitude(double amplitude_v) { amplitude_v_ = amplitude_v; }
    void setRepeat(bool repeat) { repeat_ = repeat; }

    // Envelope rise/fall time in milliseconds (default 5ms)
    void setEnvelopeTime(double ms) { envelopeTime_ms_ = ms; }

    //------------------------------------------------------------------
    // AntennaStimulus interface
    //------------------------------------------------------------------

    [[nodiscard]] double getSample(double time_s) const override;
    [[nodiscard]] std::string description() const override;
    void reset() override;
    [[nodiscard]] bool hasMore(double time_s) const override;

    // Baseband I/Q for functional simulation (avoids RF aliasing)
    void getBasebandIQ(double time_s, double lo_freq_hz,
                       double& out_i, double& out_q) const override;
    [[nodiscard]] double carrierFrequency() const override { return freq_hz_; }
    [[nodiscard]] double getEnvelope(double time_s) const override;

    //------------------------------------------------------------------
    // Accessors
    //------------------------------------------------------------------

    [[nodiscard]] double frequency() const { return freq_hz_; }
    [[nodiscard]] double amplitude() const { return amplitude_v_; }
    [[nodiscard]] double wpm() const { return wpm_; }
    [[nodiscard]] const std::string& text() const { return text_; }
    [[nodiscard]] bool repeat() const { return repeat_; }
    [[nodiscard]] double totalDuration() const { return totalDuration_s_; }

private:
    // Element types in the keying sequence
    enum class Element {
        Dit,        // Short element (1 unit)
        Dah,        // Long element (3 units)
        IntraGap,   // Gap within character (1 unit)
        CharGap,    // Gap between characters (3 units)
        WordGap     // Gap between words (7 units)
    };

    // Build the element sequence from text
    void buildSequence();

    // Get Morse pattern for a character (nullptr if not found)
    static const char* getMorsePattern(char c);

    // Calculate envelope at given position within element
    double getEnvelope(double posInElement, double elementDuration) const;

    double freq_hz_;
    double amplitude_v_;
    std::string text_;
    double wpm_;
    bool repeat_ = true;

    double ditDuration_s_;      // Duration of one dit in seconds
    double envelopeTime_ms_ = 5.0;  // Rise/fall time

    // Precomputed keying sequence
    struct KeyEvent {
        double startTime;
        double duration;
        bool keyDown;  // true = transmit, false = silence
    };
    std::vector<KeyEvent> sequence_;
    double totalDuration_s_ = 0.0;
};

} // namespace nexrx
