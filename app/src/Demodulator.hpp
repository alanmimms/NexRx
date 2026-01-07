/**
 * @file Demodulator.hpp
 * @brief SSB/AM/CW demodulator for I/Q baseband samples
 *
 * Converts complex baseband I/Q samples to audio.
 * Supports USB, LSB, AM, and CW modes.
 */

#pragma once

#include <cmath>
#include <atomic>

class Demodulator {
public:
    enum class Mode { USB, LSB, AM, CW };

    Demodulator() = default;

    /**
     * @brief Set demodulation mode
     */
    void setMode(Mode mode) { mode_ = mode; }
    Mode getMode() const { return mode_; }

    /**
     * @brief Set BFO offset for CW mode (typically 600-800 Hz)
     */
    void setBfoOffset(float hz) { bfoOffset_ = hz; }
    float getBfoOffset() const { return bfoOffset_; }

    /**
     * @brief Set sample rate (needed for CW BFO)
     */
    void setSampleRate(float rate) { sampleRate_ = rate; }

    /**
     * @brief Process single I/Q sample, return audio sample
     *
     * For a direct-conversion receiver with LO below RF:
     * - USB: positive frequencies appear as positive baseband
     * - LSB: would need LO above RF, or conjugate the signal
     *
     * @param i In-phase sample (-1.0 to +1.0)
     * @param q Quadrature sample (-1.0 to +1.0)
     * @return Audio sample
     */
    float process(float i, float q) {
        float audio = 0.0f;

        switch (mode_) {
            case Mode::USB:
                // USB: audio is the real part of baseband
                // When LO < RF, signal appears at positive baseband freq
                // Taking real part extracts the audio
                audio = i;
                break;

            case Mode::LSB:
                // LSB: conjugate the signal (negate Q) then take real
                // This flips the spectrum, making LSB audible
                // Alternatively: audio = I*cos - Q*sin for proper Weaver
                audio = i;  // For now, same as USB (proper LSB needs more)
                // TODO: Implement proper LSB with Hilbert or Weaver method
                break;

            case Mode::AM:
                // AM: envelope detection (magnitude)
                audio = std::sqrt(i * i + q * q);
                break;

            case Mode::CW:
                // CW: mix with BFO to produce audible tone
                // BFO shifts DC signal to audio frequency
                {
                    float bfoPhaseInc = 2.0f * 3.14159265f * bfoOffset_ / sampleRate_;
                    float cos_bfo = std::cos(bfoPhase_);
                    float sin_bfo = std::sin(bfoPhase_);

                    // Mix: (I + jQ) * (cos - j*sin) = I*cos + Q*sin + j(...)
                    audio = i * cos_bfo + q * sin_bfo;

                    bfoPhase_ += bfoPhaseInc;
                    if (bfoPhase_ > 2.0f * 3.14159265f) {
                        bfoPhase_ -= 2.0f * 3.14159265f;
                    }
                }
                break;
        }

        return audio;
    }

    /**
     * @brief Reset internal state (BFO phase, etc.)
     */
    void reset() {
        bfoPhase_ = 0.0f;
    }

private:
    Mode mode_ = Mode::USB;
    float bfoOffset_ = 700.0f;   // Hz, for CW
    float sampleRate_ = 96000.0f;
    float bfoPhase_ = 0.0f;      // BFO oscillator phase
};
