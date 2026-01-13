/**
 * @file Demodulator.hpp
 * @brief SSB/AM/CW demodulator for I/Q baseband samples
 *
 * Converts complex baseband I/Q samples to audio.
 * Supports USB, LSB, AM, and CW modes.
 * Includes 50Hz highpass and 5kHz lowpass audio filters.
 *
 * USB demodulation uses the phasing method with FIR Hilbert:
 * - 511-tap FIR Hilbert transform for flat response 200Hz-48kHz
 * - Gain normalized across audio band
 */

#pragma once

#include <cmath>
#include <array>
#include <iostream>

class Demodulator {
public:
    enum class Mode { USB, LSB, AM, CW };

    Demodulator() {
        computeFilterCoeffs();
        initHilbertCoeffs();
    }

    void setMode(Mode mode) { mode_ = mode; }
    Mode getMode() const { return mode_; }

    void setBfoOffset(float hz) { bfoOffset_ = hz; }
    float getBfoOffset() const { return bfoOffset_; }

    void setSampleRate(float rate) {
        sampleRate_ = rate;
        computeFilterCoeffs();
        initHilbertCoeffs();
    }

    void setFilterEnabled(bool enabled) { filterEnabled_ = enabled; }

    float process(float i, float q) {
        float audio = 0.0f;

        // Apply Hilbert transform to Q and delay I to match
        float hilbert_q = applyHilbert(q);
        float delayed_i = applyIDelay(i);

        switch (mode_) {
            case Mode::USB:
                audio = delayed_i - hilbert_q;
                break;
            case Mode::LSB:
                audio = delayed_i + hilbert_q;
                break;
            case Mode::AM:
                audio = std::sqrt(i * i + q * q);
                break;
            case Mode::CW:
                {
                    float bfoPhaseInc = 2.0f * PI * bfoOffset_ / sampleRate_;
                    float cos_bfo = std::cos(bfoPhase_);
                    float sin_bfo = std::sin(bfoPhase_);
                    audio = i * cos_bfo + q * sin_bfo;
                    bfoPhase_ += bfoPhaseInc;
                    if (bfoPhase_ > 2.0f * PI) bfoPhase_ -= 2.0f * PI;
                }
                break;
        }

        if (filterEnabled_) {
            audio = applyBandpassFilter(audio);
        }

        return audio;
    }

    void reset() {
        bfoPhase_ = 0.0f;
        for (auto& s : hpState_) s = {0.0f, 0.0f};
        for (auto& s : lpState_) s = {0.0f, 0.0f};
        hilbertDelayLine_.fill(0.0f);
        iDelayLine_.fill(0.0f);
        hilbertIdx_ = 0;
        iDelayIdx_ = 0;
    }

private:
    static constexpr float PI = 3.14159265358979323846f;

    Mode mode_ = Mode::USB;
    float bfoOffset_ = 700.0f;
    float sampleRate_ = 96000.0f;
    float bfoPhase_ = 0.0f;
    bool filterEnabled_ = true;

    // 255-tap Hilbert FIR for flat response from ~376 Hz to Nyquist
    // At 96kHz: -3dB at 96000/255 ≈ 376 Hz
    // Good balance between rejection and CPU load
    static constexpr int HILBERT_TAPS = 255;
    static constexpr int HILBERT_CENTER = HILBERT_TAPS / 2;  // = 127

    std::array<float, HILBERT_TAPS> hilbertCoeffs_ = {};
    std::array<float, HILBERT_TAPS> hilbertDelayLine_ = {};
    std::array<float, HILBERT_CENTER> iDelayLine_ = {};
    int hilbertIdx_ = 0;
    int iDelayIdx_ = 0;

    void initHilbertCoeffs() {
        // Compute ideal Hilbert coefficients with Kaiser window
        // Kaiser window gives better control over stopband than Blackman
        float beta = 8.0f;  // Kaiser beta for good stopband rejection

        for (int i = 0; i < HILBERT_TAPS; ++i) {
            int n = i - HILBERT_CENTER;
            if (n == 0) {
                hilbertCoeffs_[i] = 0.0f;
            } else if (n % 2 == 0) {
                hilbertCoeffs_[i] = 0.0f;
            } else {
                hilbertCoeffs_[i] = 2.0f / (PI * n);
            }

            // Kaiser window: I0(beta * sqrt(1 - (n/M)^2)) / I0(beta)
            float M = HILBERT_CENTER;
            float x = static_cast<float>(n) / M;
            if (std::abs(x) <= 1.0f) {
                float arg = beta * std::sqrt(1.0f - x * x);
                float window = bessel_i0(arg) / bessel_i0(beta);
                hilbertCoeffs_[i] *= window;
            } else {
                hilbertCoeffs_[i] = 0.0f;
            }
        }

        // Compute gain at multiple frequencies and find average for normalization
        float totalGain = 0.0f;
        int numFreqs = 0;
        for (float freq = 300.0f; freq <= 4000.0f; freq += 100.0f) {
            float omega = 2.0f * PI * freq / sampleRate_;
            float sumReal = 0.0f, sumImag = 0.0f;
            for (int i = 0; i < HILBERT_TAPS; ++i) {
                float phase = omega * (i - HILBERT_CENTER);
                sumReal += hilbertCoeffs_[i] * std::cos(phase);
                sumImag += hilbertCoeffs_[i] * std::sin(phase);
            }
            float gain = std::sqrt(sumReal * sumReal + sumImag * sumImag);
            totalGain += gain;
            numFreqs++;
        }
        float avgGain = totalGain / numFreqs;

        // Normalize
        if (avgGain > 0.01f) {
            float scale = 1.0f / avgGain;
            for (int i = 0; i < HILBERT_TAPS; ++i) {
                hilbertCoeffs_[i] *= scale;
            }
        }

        std::cout << "[Demodulator] " << HILBERT_TAPS << "-tap Hilbert initialized" << std::endl;
    }

    // Bessel I0 function for Kaiser window
    static float bessel_i0(float x) {
        float sum = 1.0f;
        float term = 1.0f;
        float x2 = x * x / 4.0f;
        for (int k = 1; k < 20; ++k) {
            term *= x2 / (k * k);
            sum += term;
            if (term < 1e-10f) break;
        }
        return sum;
    }

    float applyHilbert(float q) {
        hilbertDelayLine_[hilbertIdx_] = q;

        float sum = 0.0f;
        int idx = hilbertIdx_;
        for (int i = 0; i < HILBERT_TAPS; ++i) {
            sum += hilbertCoeffs_[i] * hilbertDelayLine_[idx];
            idx--;
            if (idx < 0) idx = HILBERT_TAPS - 1;
        }

        hilbertIdx_++;
        if (hilbertIdx_ >= HILBERT_TAPS) hilbertIdx_ = 0;

        return sum;
    }

    float applyIDelay(float i) {
        float delayed = iDelayLine_[iDelayIdx_];
        iDelayLine_[iDelayIdx_] = i;
        iDelayIdx_ = (iDelayIdx_ + 1) % HILBERT_CENTER;
        return delayed;
    }

    // Audio bandpass filter
    static constexpr int LP_STAGES = 2;
    static constexpr int HP_STAGES = 1;

    struct BiquadCoeffs {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
    };

    struct BiquadState {
        float z1 = 0.0f, z2 = 0.0f;
    };

    std::array<BiquadCoeffs, HP_STAGES> hpCoeffs_;
    std::array<BiquadState, HP_STAGES> hpState_;
    std::array<BiquadCoeffs, LP_STAGES> lpCoeffs_;
    std::array<BiquadState, LP_STAGES> lpState_;

    static float processBiquad(float x, const BiquadCoeffs& c, BiquadState& s) {
        float y = c.b0 * x + s.z1;
        s.z1 = c.b1 * x - c.a1 * y + s.z2;
        s.z2 = c.b2 * x - c.a2 * y;
        return y;
    }

    void computeFilterCoeffs() {
        // 50Hz highpass
        {
            constexpr float fc = 50.0f;
            float omega = 2.0f * PI * fc / sampleRate_;
            float sn = std::sin(omega);
            float cs = std::cos(omega);
            float alpha = sn / (2.0f * 0.7071067812f);

            float a0 = 1.0f + alpha;
            hpCoeffs_[0].b0 = (1.0f + cs) / 2.0f / a0;
            hpCoeffs_[0].b1 = -(1.0f + cs) / a0;
            hpCoeffs_[0].b2 = (1.0f + cs) / 2.0f / a0;
            hpCoeffs_[0].a1 = -2.0f * cs / a0;
            hpCoeffs_[0].a2 = (1.0f - alpha) / a0;
        }

        // 5kHz lowpass (4th order)
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

    float applyBandpassFilter(float x) {
        float y = x;
        for (int i = 0; i < HP_STAGES; ++i) {
            y = processBiquad(y, hpCoeffs_[i], hpState_[i]);
        }
        for (int i = 0; i < LP_STAGES; ++i) {
            y = processBiquad(y, lpCoeffs_[i], lpState_[i]);
        }
        return y;
    }
};
