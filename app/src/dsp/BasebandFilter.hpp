/**
 * @file BasebandFilter.hpp
 * @brief Complex FIR bandpass and notch filters for baseband I/Q processing
 *
 * Provides dynamically adjustable filters with:
 * - Linear phase (FIR)
 * - Arbitrary center frequency and bandwidth
 * - Adjustable steepness via tap count
 */

#pragma once

#include <vector>
#include <cmath>
#include <algorithm>

namespace nexrx {

class BasebandFilter {
public:
    explicit BasebandFilter(float sampleRate = 96000.0f);

    // Process complex I/Q sample (in-place)
    void process(float& i, float& q);

    // Bandpass control
    void setBandpassEnabled(bool en);
    void setBandpassCenter(float hz);   // Offset from DC (-fs/2 to +fs/2)
    void setBandpassWidth(float hz);    // 3dB bandwidth
    void setBandpassTaps(int taps);     // Filter length (must be odd)

    // Notch control
    void setNotchEnabled(bool en);
    void setNotchCenter(float hz);      // Offset from DC
    void setNotchWidth(float hz);       // Notch bandwidth

    // Getters
    bool bandpassEnabled() const { return bpEnabled_; }
    float bandpassCenter() const { return bpCenter_; }
    float bandpassWidth() const { return bpWidth_; }
    int bandpassTaps() const { return bpTaps_; }

    bool notchEnabled() const { return notchEnabled_; }
    float notchCenter() const { return notchCenter_; }
    float notchWidth() const { return notchWidth_; }

private:
    float sampleRate_;

    // Bandpass FIR state
    bool bpEnabled_ = false;
    float bpCenter_ = 700.0f;
    float bpWidth_ = 500.0f;
    int bpTaps_ = 127;
    std::vector<float> bpCoeffsI_;      // Complex FIR coefficients (I component)
    std::vector<float> bpCoeffsQ_;      // Complex FIR coefficients (Q component)
    std::vector<float> bpHistoryI_;     // Circular delay line for I
    std::vector<float> bpHistoryQ_;     // Circular delay line for Q
    size_t bpIndex_ = 0;
    bool bpCoeffsDirty_ = true;

    // Notch FIR state
    bool notchEnabled_ = false;
    float notchCenter_ = 0.0f;
    float notchWidth_ = 100.0f;
    int notchTaps_ = 63;
    std::vector<float> notchCoeffsI_;
    std::vector<float> notchCoeffsQ_;
    std::vector<float> notchHistoryI_;
    std::vector<float> notchHistoryQ_;
    size_t notchIndex_ = 0;
    bool notchCoeffsDirty_ = true;

    // Crossfade state for smooth transitions
    static constexpr int kCrossfadeSamples = 960;  // 10ms at 96kHz
    bool bpCrossfading_ = false;
    int bpCrossfadePos_ = 0;
    std::vector<float> bpOldCoeffsI_;
    std::vector<float> bpOldCoeffsQ_;
    std::vector<float> bpOldHistoryI_;
    std::vector<float> bpOldHistoryQ_;
    size_t bpOldIndex_ = 0;
    int bpOldTaps_ = 0;

    bool notchCrossfading_ = false;
    int notchCrossfadePos_ = 0;
    std::vector<float> notchOldCoeffsI_;
    std::vector<float> notchOldCoeffsQ_;
    std::vector<float> notchOldHistoryI_;
    std::vector<float> notchOldHistoryQ_;
    size_t notchOldIndex_ = 0;

    // Filter design functions
    void designBandpass();
    void designNotch();

    // Start crossfade from current to new coefficients
    void startBandpassCrossfade();
    void startNotchCrossfade();

    // Process with old coefficients (during crossfade)
    void processOldBandpass(float i, float q, float& outI, float& outQ);
    void processOldNotch(float i, float q, float& outI, float& outQ);

    // Kaiser window function
    static float kaiser(int n, int N, float beta);

    // Modified Bessel function I0 (for Kaiser window)
    static float bessel_i0(float x);
};

} // namespace nexrx
