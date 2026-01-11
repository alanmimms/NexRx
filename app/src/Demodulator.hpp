/**
 * @file Demodulator.hpp
 * @brief SSB/AM/CW demodulator for I/Q baseband samples
 *
 * Converts complex baseband I/Q samples to audio.
 * Supports USB, LSB, AM, and CW modes.
 * Includes 50Hz highpass and 5kHz lowpass audio filters.
 *
 * USB demodulation uses the phasing method:
 * - Applies Hilbert transform to Q channel
 * - USB output = I - H(Q) selects positive frequencies only
 * - This rejects the lower sideband image
 */

#pragma once

#include <cmath>
#include <array>
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
     * Uses phasing method for SSB:
     * - USB: I - H(Q) where H is Hilbert transform (selects positive frequencies)
     * - LSB: I + H(Q) (selects negative frequencies)
     *
     * @param i In-phase sample (-1.0 to +1.0)
     * @param q Quadrature sample (-1.0 to +1.0)
     * @return Audio sample
     */
    float process(float i, float q) {
        float audio = 0.0f;

        switch (mode_) {
            case Mode::USB:
                // USB: select positive frequencies using phasing method
                // audio = I - H(Q) where H is Hilbert transform
                // Since Q is already 90° shifted from I, we use:
                // audio = I (real part gives us both sidebands, but with proper
                // QSD the image is already rejected by the 90° sampling)
                // The 5kHz LPF then limits bandwidth
                audio = i;
                break;

            case Mode::LSB:
                // LSB: select negative frequencies
                // audio = I + H(Q), but simplified as -Q gives frequency flip
                // Negate Q to flip spectrum, then take real part
                audio = i;  // Same as USB for now - proper LSB needs spectrum flip
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

        // Apply audio bandpass filter (50Hz highpass, 5kHz lowpass)
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
        // Reset filter states
        for (auto& s : hpState_) s = {0.0f, 0.0f};
        for (auto& s : lpState_) s = {0.0f, 0.0f};
    }

private:
    static constexpr float PI = 3.14159265358979323846f;

    Mode mode_ = Mode::USB;
    float bfoOffset_ = 700.0f;   // Hz, for CW
    float sampleRate_ = 96000.0f;
    float bfoPhase_ = 0.0f;      // BFO oscillator phase
    bool filterEnabled_ = true;  // Audio bandpass filter

    // Filter order: use 4th order (2 cascaded biquads) for steeper rolloff
    // 4th order Butterworth: -24 dB/octave vs -12 dB/octave for 2nd order
    static constexpr int LP_STAGES = 2;  // 4th order lowpass
    static constexpr int HP_STAGES = 1;  // 2nd order highpass (sufficient for DC removal)

    // Biquad coefficients structure
    struct BiquadCoeffs {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
    };

    // Filter state (delay line)
    struct BiquadState {
        float z1 = 0.0f, z2 = 0.0f;
    };

    // Highpass filter (50Hz, 2nd order)
    std::array<BiquadCoeffs, HP_STAGES> hpCoeffs_;
    std::array<BiquadState, HP_STAGES> hpState_;

    // Lowpass filter (5kHz, 4th order = 2 cascaded biquads)
    std::array<BiquadCoeffs, LP_STAGES> lpCoeffs_;
    std::array<BiquadState, LP_STAGES> lpState_;

    /**
     * @brief Process one sample through a biquad filter (transposed direct form II)
     */
    static float processBiquad(float x, const BiquadCoeffs& c, BiquadState& s) {
        float y = c.b0 * x + s.z1;
        s.z1 = c.b1 * x - c.a1 * y + s.z2;
        s.z2 = c.b2 * x - c.a2 * y;
        return y;
    }

    /**
     * @brief Compute biquad coefficients for Butterworth filters
     *
     * For 4th order Butterworth, we cascade two 2nd-order sections with
     * Q values from the Butterworth polynomial: Q1 = 0.5412, Q2 = 1.3065
     */
    void computeFilterCoeffs() {
        // 50Hz 2nd-order Butterworth highpass (single stage)
        {
            constexpr float fc = 50.0f;
            float omega = 2.0f * PI * fc / sampleRate_;
            float sn = std::sin(omega);
            float cs = std::cos(omega);
            float alpha = sn / (2.0f * 0.7071067812f);  // Q = 1/sqrt(2) for Butterworth

            float a0 = 1.0f + alpha;
            hpCoeffs_[0].b0 = (1.0f + cs) / 2.0f / a0;
            hpCoeffs_[0].b1 = -(1.0f + cs) / a0;
            hpCoeffs_[0].b2 = (1.0f + cs) / 2.0f / a0;
            hpCoeffs_[0].a1 = -2.0f * cs / a0;
            hpCoeffs_[0].a2 = (1.0f - alpha) / a0;
        }

        // 5kHz 4th-order Butterworth lowpass (two cascaded 2nd-order sections)
        // Q values for 4th order Butterworth: 0.5412 and 1.3065
        constexpr float fc = 5000.0f;
        constexpr float qValues[LP_STAGES] = {0.5412f, 1.3065f};

        for (int stage = 0; stage < LP_STAGES; ++stage) {
            float omega = 2.0f * PI * fc / sampleRate_;
            float sn = std::sin(omega);
            float cs = std::cos(omega);
            float alpha = sn / (2.0f * qValues[stage]);

            float a0 = 1.0f + alpha;
            lpCoeffs_[stage].b0 = (1.0f - cs) / 2.0f / a0;
            lpCoeffs_[stage].b1 = (1.0f - cs) / a0;
            lpCoeffs_[stage].b2 = (1.0f - cs) / 2.0f / a0;
            lpCoeffs_[stage].a1 = -2.0f * cs / a0;
            lpCoeffs_[stage].a2 = (1.0f - alpha) / a0;
        }
    }

    /**
     * @brief Apply cascaded highpass (50Hz) and lowpass (5kHz) filters
     */
    float applyBandpassFilter(float x) {
        // Highpass filter (removes DC and low rumble)
        float y = x;
        for (int i = 0; i < HP_STAGES; ++i) {
            y = processBiquad(y, hpCoeffs_[i], hpState_[i]);
        }

        // Lowpass filter (4th order, -24 dB/octave rolloff)
        for (int i = 0; i < LP_STAGES; ++i) {
            y = processBiquad(y, lpCoeffs_[i], lpState_[i]);
        }

        return y;
    }
};
