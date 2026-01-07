/**
 * @file Demodulator.hpp
 * @brief SSB/AM/CW demodulator for I/Q baseband samples
 *
 * Converts complex baseband I/Q samples to audio.
 * Supports USB, LSB, AM, and CW modes.
 * Includes 50Hz-5kHz audio bandpass filter.
 */

#pragma once

#include <cmath>
#include <atomic>

class Demodulator {
public:
    enum class Mode { USB, LSB, AM, CW };

    Demodulator() {
        // Initialize filter coefficients for default sample rate
        computeFilterCoeffs();
    }

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
     * @brief Set sample rate (needed for CW BFO and audio filters)
     */
    void setSampleRate(float rate) {
        sampleRate_ = rate;
        computeFilterCoeffs();
    }

    /**
     * @brief Enable/disable audio bandpass filter (50Hz-5kHz)
     */
    void setFilterEnabled(bool enabled) { filterEnabled_ = enabled; }

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

        // Apply audio bandpass filter (50Hz-5kHz)
        if (filterEnabled_) {
            audio = applyBandpassFilter(audio);
        }

        return audio;
    }

    /**
     * @brief Reset internal state (BFO phase, filters, etc.)
     */
    void reset() {
        bfoPhase_ = 0.0f;
        // Reset filter state
        hpState_[0] = hpState_[1] = 0.0f;
        lpState_[0] = lpState_[1] = 0.0f;
    }

private:
    Mode mode_ = Mode::USB;
    float bfoOffset_ = 700.0f;   // Hz, for CW
    float sampleRate_ = 96000.0f;
    float bfoPhase_ = 0.0f;      // BFO oscillator phase
    bool filterEnabled_ = true;  // Audio bandpass filter

    // Biquad filter coefficients for 50Hz highpass
    float hpB0_ = 1.0f, hpB1_ = 0.0f, hpB2_ = 0.0f;
    float hpA1_ = 0.0f, hpA2_ = 0.0f;
    float hpState_[2] = {0.0f, 0.0f};

    // Biquad filter coefficients for 5kHz lowpass
    float lpB0_ = 1.0f, lpB1_ = 0.0f, lpB2_ = 0.0f;
    float lpA1_ = 0.0f, lpA2_ = 0.0f;
    float lpState_[2] = {0.0f, 0.0f};

    /**
     * @brief Compute biquad coefficients for Butterworth filters
     */
    void computeFilterCoeffs() {
        // 50Hz 2nd-order Butterworth highpass
        {
            constexpr float fc = 50.0f;
            float omega = 2.0f * 3.14159265f * fc / sampleRate_;
            float sn = std::sin(omega);
            float cs = std::cos(omega);
            float alpha = sn / (2.0f * 0.7071067812f);  // Q = 1/sqrt(2) for Butterworth

            float a0 = 1.0f + alpha;
            hpB0_ = (1.0f + cs) / 2.0f / a0;
            hpB1_ = -(1.0f + cs) / a0;
            hpB2_ = (1.0f + cs) / 2.0f / a0;
            hpA1_ = -2.0f * cs / a0;
            hpA2_ = (1.0f - alpha) / a0;
        }

        // 5kHz 2nd-order Butterworth lowpass
        {
            constexpr float fc = 5000.0f;
            float omega = 2.0f * 3.14159265f * fc / sampleRate_;
            float sn = std::sin(omega);
            float cs = std::cos(omega);
            float alpha = sn / (2.0f * 0.7071067812f);  // Q = 1/sqrt(2) for Butterworth

            float a0 = 1.0f + alpha;
            lpB0_ = (1.0f - cs) / 2.0f / a0;
            lpB1_ = (1.0f - cs) / a0;
            lpB2_ = (1.0f - cs) / 2.0f / a0;
            lpA1_ = -2.0f * cs / a0;
            lpA2_ = (1.0f - alpha) / a0;
        }
    }

    /**
     * @brief Apply cascaded highpass (50Hz) and lowpass (5kHz) biquad filters
     */
    float applyBandpassFilter(float x) {
        // Highpass filter (removes DC and low rumble)
        float hp_out = hpB0_ * x + hpState_[0];
        hpState_[0] = hpB1_ * x - hpA1_ * hp_out + hpState_[1];
        hpState_[1] = hpB2_ * x - hpA2_ * hp_out;

        // Lowpass filter (removes high frequency noise)
        float lp_out = lpB0_ * hp_out + lpState_[0];
        lpState_[0] = lpB1_ * hp_out - lpA1_ * lp_out + lpState_[1];
        lpState_[1] = lpB2_ * hp_out - lpA2_ * lp_out;

        return lp_out;
    }
};
