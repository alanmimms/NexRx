/**
 * @file BasebandFilter.cpp
 * @brief Complex FIR bandpass and notch filter implementation
 */

#include "BasebandFilter.hpp"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace nexrx {

BasebandFilter::BasebandFilter(float sampleRate)
    : sampleRate_(sampleRate) {
    // Initialize history buffers
    bpHistoryI_.resize(bpTaps_, 0.0f);
    bpHistoryQ_.resize(bpTaps_, 0.0f);
    notchHistoryI_.resize(notchTaps_, 0.0f);
    notchHistoryQ_.resize(notchTaps_, 0.0f);
}

void BasebandFilter::setBandpassEnabled(bool en) {
    bpEnabled_ = en;
}

void BasebandFilter::setBandpassCenter(float hz) {
    if (bpCenter_ != hz) {
        if (bpEnabled_ && !bpCoeffsI_.empty()) {
            startBandpassCrossfade();
        }
        bpCenter_ = hz;
        bpCoeffsDirty_ = true;
    }
}

void BasebandFilter::setBandpassWidth(float hz) {
    if (bpWidth_ != hz) {
        if (bpEnabled_ && !bpCoeffsI_.empty()) {
            startBandpassCrossfade();
        }
        bpWidth_ = std::max(10.0f, hz);  // Minimum 10 Hz
        bpCoeffsDirty_ = true;
    }
}

void BasebandFilter::setBandpassTaps(int taps) {
    // Ensure odd number of taps for symmetric FIR
    taps = taps | 1;
    taps = std::clamp(taps, 15, 1023);

    if (bpTaps_ != taps) {
        if (bpEnabled_ && !bpCoeffsI_.empty()) {
            startBandpassCrossfade();
        }
        bpTaps_ = taps;
        bpHistoryI_.resize(taps, 0.0f);
        bpHistoryQ_.resize(taps, 0.0f);
        bpIndex_ = 0;
        bpCoeffsDirty_ = true;
    }
}

void BasebandFilter::setNotchEnabled(bool en) {
    notchEnabled_ = en;
}

void BasebandFilter::setNotchCenter(float hz) {
    if (notchCenter_ != hz) {
        if (notchEnabled_ && !notchCoeffsI_.empty()) {
            startNotchCrossfade();
        }
        notchCenter_ = hz;
        notchCoeffsDirty_ = true;
    }
}

void BasebandFilter::setNotchWidth(float hz) {
    if (notchWidth_ != hz) {
        if (notchEnabled_ && !notchCoeffsI_.empty()) {
            startNotchCrossfade();
        }
        notchWidth_ = std::max(10.0f, hz);
        notchCoeffsDirty_ = true;
    }
}

void BasebandFilter::process(float& i, float& q) {
    // Recompute coefficients if parameters changed
    if (bpEnabled_ && bpCoeffsDirty_) {
        designBandpass();
        bpCoeffsDirty_ = false;
    }
    if (notchEnabled_ && notchCoeffsDirty_) {
        designNotch();
        notchCoeffsDirty_ = false;
    }

    if (bpEnabled_) {
        // Store input in circular history buffer
        bpHistoryI_[bpIndex_] = i;
        bpHistoryQ_[bpIndex_] = q;

        // Complex FIR convolution
        // Output = input ⊛ (coeffsI + j*coeffsQ)
        // (a + jb) * (c + jd) = (ac - bd) + j(ad + bc)
        float outI = 0.0f, outQ = 0.0f;
        size_t idx = bpIndex_;

        for (int n = 0; n < bpTaps_; n++) {
            float hi = bpHistoryI_[idx];
            float hq = bpHistoryQ_[idx];
            float cI = bpCoeffsI_[n];
            float cQ = bpCoeffsQ_[n];

            // Complex multiply: (hi + j*hq) * (cI + j*cQ)
            outI += hi * cI - hq * cQ;
            outQ += hi * cQ + hq * cI;

            // Decrement with wrap
            if (idx == 0) idx = bpTaps_ - 1;
            else idx--;
        }

        // Handle crossfade if active
        if (bpCrossfading_) {
            float oldI, oldQ;
            processOldBandpass(i, q, oldI, oldQ);

            // Linear crossfade from old to new
            float alpha = static_cast<float>(bpCrossfadePos_) / kCrossfadeSamples;
            outI = (1.0f - alpha) * oldI + alpha * outI;
            outQ = (1.0f - alpha) * oldQ + alpha * outQ;

            bpCrossfadePos_++;
            if (bpCrossfadePos_ >= kCrossfadeSamples) {
                bpCrossfading_ = false;
                bpOldCoeffsI_.clear();
                bpOldCoeffsQ_.clear();
                bpOldHistoryI_.clear();
                bpOldHistoryQ_.clear();
            }
        }

        i = outI;
        q = outQ;
        bpIndex_ = (bpIndex_ + 1) % bpTaps_;
    }

    if (notchEnabled_) {
        // Store input in circular history buffer
        notchHistoryI_[notchIndex_] = i;
        notchHistoryQ_[notchIndex_] = q;

        // Compute bandpass output at notch frequency
        float bpI = 0.0f, bpQ = 0.0f;
        size_t idx = notchIndex_;

        for (int n = 0; n < notchTaps_; n++) {
            float hi = notchHistoryI_[idx];
            float hq = notchHistoryQ_[idx];
            float cI = notchCoeffsI_[n];
            float cQ = notchCoeffsQ_[n];

            bpI += hi * cI - hq * cQ;
            bpQ += hi * cQ + hq * cI;

            if (idx == 0) idx = notchTaps_ - 1;
            else idx--;
        }

        // Notch = delayed input - bandpass
        // Get delayed input (center of filter = group delay)
        int delay = notchTaps_ / 2;
        size_t delayIdx = (notchIndex_ + notchTaps_ - delay) % notchTaps_;

        float outI = notchHistoryI_[delayIdx] - bpI;
        float outQ = notchHistoryQ_[delayIdx] - bpQ;

        // Handle crossfade if active
        if (notchCrossfading_) {
            float oldI, oldQ;
            processOldNotch(i, q, oldI, oldQ);

            // Linear crossfade from old to new
            float alpha = static_cast<float>(notchCrossfadePos_) / kCrossfadeSamples;
            outI = (1.0f - alpha) * oldI + alpha * outI;
            outQ = (1.0f - alpha) * oldQ + alpha * outQ;

            notchCrossfadePos_++;
            if (notchCrossfadePos_ >= kCrossfadeSamples) {
                notchCrossfading_ = false;
                notchOldCoeffsI_.clear();
                notchOldCoeffsQ_.clear();
                notchOldHistoryI_.clear();
                notchOldHistoryQ_.clear();
            }
        }

        i = outI;
        q = outQ;

        notchIndex_ = (notchIndex_ + 1) % notchTaps_;
    }
}

void BasebandFilter::designBandpass() {
    // Design complex bandpass FIR using windowed-sinc method
    // 1. Design lowpass with cutoff = bandwidth/2
    // 2. Modulate to center frequency

    bpCoeffsI_.resize(bpTaps_);
    bpCoeffsQ_.resize(bpTaps_);

    int center = bpTaps_ / 2;
    float cutoff = bpWidth_ / (2.0f * sampleRate_);  // Normalized cutoff

    // Kaiser window beta for ~60 dB stopband attenuation
    float beta = 6.0f;

    // Step 1: Design lowpass prototype
    std::vector<float> lpfCoeffs(bpTaps_);

    for (int n = 0; n < bpTaps_; n++) {
        int k = n - center;

        // Sinc lowpass
        float h;
        if (k == 0) {
            h = 2.0f * cutoff;  // Value at center
        } else {
            float x = 2.0f * M_PI * cutoff * k;
            h = std::sin(x) / (M_PI * k);
        }

        // Apply Kaiser window
        float w = kaiser(n, bpTaps_, beta);
        lpfCoeffs[n] = h * w;
    }

    // Step 2: Modulate to center frequency (complex exponential)
    // coeffs[n] = lpf[n] * exp(j * 2π * fc * (n - center) / fs)
    for (int n = 0; n < bpTaps_; n++) {
        float phase = 2.0f * M_PI * bpCenter_ * (n - center) / sampleRate_;
        bpCoeffsI_[n] = lpfCoeffs[n] * std::cos(phase);
        bpCoeffsQ_[n] = lpfCoeffs[n] * std::sin(phase);
    }
}

void BasebandFilter::designNotch() {
    // Design narrow bandpass at notch frequency
    // Notch output = input - bandpass (computed in process())

    notchCoeffsI_.resize(notchTaps_);
    notchCoeffsQ_.resize(notchTaps_);

    int center = notchTaps_ / 2;
    float cutoff = notchWidth_ / (2.0f * sampleRate_);

    // Higher beta for narrower notch with better rejection
    float beta = 8.0f;

    // Design lowpass prototype
    std::vector<float> lpfCoeffs(notchTaps_);

    for (int n = 0; n < notchTaps_; n++) {
        int k = n - center;

        float h;
        if (k == 0) {
            h = 2.0f * cutoff;
        } else {
            float x = 2.0f * M_PI * cutoff * k;
            h = std::sin(x) / (M_PI * k);
        }

        float w = kaiser(n, notchTaps_, beta);
        lpfCoeffs[n] = h * w;
    }

    // Modulate to notch center frequency
    for (int n = 0; n < notchTaps_; n++) {
        float phase = 2.0f * M_PI * notchCenter_ * (n - center) / sampleRate_;
        notchCoeffsI_[n] = lpfCoeffs[n] * std::cos(phase);
        notchCoeffsQ_[n] = lpfCoeffs[n] * std::sin(phase);
    }
}

float BasebandFilter::kaiser(int n, int N, float beta) {
    // Kaiser window: w[n] = I0(beta * sqrt(1 - ((n - M) / M)^2)) / I0(beta)
    // where M = (N - 1) / 2
    float M = (N - 1) / 2.0f;
    float ratio = (n - M) / M;
    float arg = beta * std::sqrt(1.0f - ratio * ratio);
    return bessel_i0(arg) / bessel_i0(beta);
}

float BasebandFilter::bessel_i0(float x) {
    // Modified Bessel function of the first kind, order 0
    // Using series expansion: I0(x) = sum_{k=0}^inf ((x/2)^k / k!)^2
    float sum = 1.0f;
    float term = 1.0f;
    float x2 = x * x / 4.0f;

    for (int k = 1; k <= 25; k++) {
        term *= x2 / (k * k);
        sum += term;
        if (term < 1e-10f * sum) break;
    }

    return sum;
}

void BasebandFilter::startBandpassCrossfade() {
    // Save current filter state for crossfading
    if (bpCrossfading_) {
        // Already crossfading - just restart with current "new" as old
    }
    bpOldCoeffsI_ = bpCoeffsI_;
    bpOldCoeffsQ_ = bpCoeffsQ_;
    bpOldHistoryI_ = bpHistoryI_;
    bpOldHistoryQ_ = bpHistoryQ_;
    bpOldIndex_ = bpIndex_;
    bpOldTaps_ = bpTaps_;
    bpCrossfadePos_ = 0;
    bpCrossfading_ = true;
}

void BasebandFilter::startNotchCrossfade() {
    // Save current filter state for crossfading
    notchOldCoeffsI_ = notchCoeffsI_;
    notchOldCoeffsQ_ = notchCoeffsQ_;
    notchOldHistoryI_ = notchHistoryI_;
    notchOldHistoryQ_ = notchHistoryQ_;
    notchOldIndex_ = notchIndex_;
    notchCrossfadePos_ = 0;
    notchCrossfading_ = true;
}

void BasebandFilter::processOldBandpass(float i, float q, float& outI, float& outQ) {
    // Process using old coefficients (during crossfade)
    if (bpOldCoeffsI_.empty() || bpOldTaps_ == 0) {
        outI = i;
        outQ = q;
        return;
    }

    // Store input in old history buffer
    bpOldHistoryI_[bpOldIndex_] = i;
    bpOldHistoryQ_[bpOldIndex_] = q;

    // FIR convolution with old coefficients
    outI = 0.0f;
    outQ = 0.0f;
    size_t idx = bpOldIndex_;

    for (int n = 0; n < bpOldTaps_; n++) {
        float hi = bpOldHistoryI_[idx];
        float hq = bpOldHistoryQ_[idx];
        float cI = bpOldCoeffsI_[n];
        float cQ = bpOldCoeffsQ_[n];

        outI += hi * cI - hq * cQ;
        outQ += hi * cQ + hq * cI;

        if (idx == 0) idx = bpOldTaps_ - 1;
        else idx--;
    }

    bpOldIndex_ = (bpOldIndex_ + 1) % bpOldTaps_;
}

void BasebandFilter::processOldNotch(float i, float q, float& outI, float& outQ) {
    // Process using old notch coefficients (during crossfade)
    if (notchOldCoeffsI_.empty()) {
        outI = i;
        outQ = q;
        return;
    }

    int oldTaps = static_cast<int>(notchOldCoeffsI_.size());

    // Store input in old history buffer
    notchOldHistoryI_[notchOldIndex_] = i;
    notchOldHistoryQ_[notchOldIndex_] = q;

    // Compute bandpass at old notch frequency
    float bpI = 0.0f, bpQ = 0.0f;
    size_t idx = notchOldIndex_;

    for (int n = 0; n < oldTaps; n++) {
        float hi = notchOldHistoryI_[idx];
        float hq = notchOldHistoryQ_[idx];
        float cI = notchOldCoeffsI_[n];
        float cQ = notchOldCoeffsQ_[n];

        bpI += hi * cI - hq * cQ;
        bpQ += hi * cQ + hq * cI;

        if (idx == 0) idx = oldTaps - 1;
        else idx--;
    }

    // Notch = delayed input - bandpass
    int delay = oldTaps / 2;
    size_t delayIdx = (notchOldIndex_ + oldTaps - delay) % oldTaps;

    outI = notchOldHistoryI_[delayIdx] - bpI;
    outQ = notchOldHistoryQ_[delayIdx] - bpQ;

    notchOldIndex_ = (notchOldIndex_ + 1) % oldTaps;
}

} // namespace nexrx
