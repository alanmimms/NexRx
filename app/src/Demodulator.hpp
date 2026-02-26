/**
 * @file Demodulator.hpp
 * @brief SSB/AM/CW demodulator for I/Q baseband samples
 *
 * Converts complex baseband I/Q samples to audio.
 * Supports USB, LSB, AM, and CW modes.
 * Includes 50Hz highpass and 5kHz lowpass audio filters.
 *
 * SSB demodulation uses FFT-based frequency-domain processing:
 * - Mathematically perfect image rejection (limited only by float precision)
 * - 512-sample blocks with 50% overlap-add for continuous audio
 * - ~5ms latency at 96kHz sample rate
 */

#pragma once

#include <cmath>
#include <array>
#include <vector>
#include <iostream>

class Demodulator {
public:
    enum class Mode { USB, LSB, AM, CW };

    // FFT block size - 512 gives good frequency resolution with ~5ms latency
    static constexpr int FFT_SIZE = 512;
    static constexpr int HOP_SIZE = FFT_SIZE / 2;  // 50% overlap

    Demodulator() {
        computeFilterCoeffs();
        initFftWindow();
        // Pre-size vectors
        fftRe_.resize(FFT_SIZE);
        fftIm_.resize(FFT_SIZE);
        outputBuffer_.resize(HOP_SIZE, 0.0f);
        overlapBuffer_.resize(HOP_SIZE, 0.0f);
        inputI_.resize(FFT_SIZE, 0.0f);
        inputQ_.resize(FFT_SIZE, 0.0f);
    }

    void setMode(Mode mode) { mode_ = mode; }
    Mode getMode() const { return mode_; }

    void setBfoOffset(float hz) { bfoOffset_ = hz; }
    float getBfoOffset() const { return bfoOffset_; }

    void setSampleRate(float rate) {
        sampleRate_ = rate;
        computeFilterCoeffs();
        initFftWindow();
    }

    void setFilterEnabled(bool enabled) { filterEnabled_ = enabled; }

    float getSignalLevelRms() {
        float rms = signalLevelRms_.exchange(0.0f);
        return rms > 0 ? rms : lastValidRms_;
    }

    // Process a single I/Q sample, returns audio sample
    // For SSB modes, uses FFT-based processing with overlap-add
    // Returns 0 while buffering, then outputs HOP_SIZE samples worth of audio
    float process(float i, float q) {
        // AM uses complex LPF for selectivity before magnitude detection
        if (mode_ == Mode::AM) {
            float fi = i, fq = q;
            // Apply 4th-order complex LPF (3kHz cutoff)
            for (int s = 0; s < LP_STAGES; ++s) {
                fi = processBiquad(fi, lpCoeffs_[s], amStateI_[s]);
                fq = processBiquad(fq, lpCoeffs_[s], amStateQ_[s]);
            }
            float mag = std::sqrt(fi * fi + fq * fq);
            updateRms(mag);
            return filterEnabled_ ? applyBandpassFilter(mag) : mag;
        }
        if (mode_ == Mode::CW) {
            float bfoPhaseInc = 2.0f * PI * bfoOffset_ / sampleRate_;
            float cos_bfo = std::cos(bfoPhase_);
            float sin_bfo = std::sin(bfoPhase_);
            float audio = i * cos_bfo + q * sin_bfo;
            updateRms(audio * 1.41421356f); // Scale to complex equivalent power
            bfoPhase_ += bfoPhaseInc;
            if (bfoPhase_ > 2.0f * PI) bfoPhase_ -= 2.0f * PI;
            return filterEnabled_ ? applyBandpassFilter(audio) : audio;
        }

        // SSB modes: buffer samples for FFT processing
        inputI_[inputPos_] = i;
        inputQ_[inputPos_] = q;
        inputPos_++;

        // When we have a full block, process it
        if (inputPos_ >= FFT_SIZE) {
            processBlock();
            inputPos_ = HOP_SIZE;  // Keep last half for overlap
            // Shift input buffer: copy second half to first half
            for (int j = 0; j < HOP_SIZE; ++j) {
                inputI_[j] = inputI_[j + HOP_SIZE];
                inputQ_[j] = inputQ_[j + HOP_SIZE];
            }
            outputPos_ = 0;  // Reset output position
        }

        // Output from output buffer
        float audio = 0.0f;
        if (outputPos_ < HOP_SIZE) {
            audio = outputBuffer_[outputPos_++];
            if (filterEnabled_) {
                audio = applyBandpassFilter(audio);
            }
        }

        return audio;
    }

    // Batch processing for efficiency - process multiple samples at once
    void processBatch(const float* iSamples, const float* qSamples,
                      float* audioOut, int count) {
        for (int n = 0; n < count; ++n) {
            audioOut[n] = process(iSamples[n], qSamples[n]);
        }
    }

    void reset() {
        bfoPhase_ = 0.0f;
        for (auto& s : hpState_) s = {0.0f, 0.0f};
        for (auto& s : lpState_) s = {0.0f, 0.0f};
        for (auto& s : amStateI_) s = {0.0f, 0.0f};
        for (auto& s : amStateQ_) s = {0.0f, 0.0f};
        std::fill(inputI_.begin(), inputI_.end(), 0.0f);
        std::fill(inputQ_.begin(), inputQ_.end(), 0.0f);
        std::fill(outputBuffer_.begin(), outputBuffer_.end(), 0.0f);
        std::fill(overlapBuffer_.begin(), overlapBuffer_.end(), 0.0f);
        inputPos_ = 0;
        outputPos_ = HOP_SIZE;  // Start with empty output buffer
    }

private:
    static constexpr float PI = 3.14159265358979323846f;

    Mode mode_ = Mode::USB;
    float bfoOffset_ = 700.0f;
    float sampleRate_ = 96000.0f;
    float bfoPhase_ = 0.0f;
    bool filterEnabled_ = true;

    // Metering
    std::atomic<float> signalLevelRms_{0.0f};
    float lastValidRms_ = 0.0f;
    float rmsAccum_ = 0.0f;
    int sampleAccum_ = 0;

    void updateRms(float val, int numSamples = 1) {
        rmsAccum_ += val * val * numSamples;
        sampleAccum_ += numSamples;
        if (sampleAccum_ >= 960) { // Update every 10ms at 96kHz
            float rms = std::sqrt(rmsAccum_ / sampleAccum_);
            signalLevelRms_.store(rms, std::memory_order_relaxed);
            lastValidRms_ = rms;
            rmsAccum_ = 0;
            sampleAccum_ = 0;
        }
    }

    // FFT-based SSB demodulation buffers
    std::vector<float> inputI_;         // Input I buffer (FFT_SIZE)
    std::vector<float> inputQ_;         // Input Q buffer (FFT_SIZE)
    std::vector<float> fftRe_;          // FFT real part
    std::vector<float> fftIm_;          // FFT imaginary part
    std::vector<float> outputBuffer_;   // Output audio buffer (HOP_SIZE)
    std::vector<float> overlapBuffer_;  // Overlap storage for next block (HOP_SIZE)
    std::array<float, FFT_SIZE> window_ = {};  // Hann window
    int inputPos_ = 0;                  // Write position in input buffer
    int outputPos_ = HOP_SIZE;          // Read position in output buffer

    void initFftWindow() {
        // Hann window for overlap-add (satisfies COLA with 50% overlap)
        for (int n = 0; n < FFT_SIZE; ++n) {
            window_[n] = 0.5f * (1.0f - std::cos(2.0f * PI * n / FFT_SIZE));
        }
        std::cout << "[Demodulator] FFT-based SSB initialized, block=" << FFT_SIZE
                  << " (" << (1000.0f * FFT_SIZE / sampleRate_) << "ms)" << std::endl;
    }

    // In-place radix-2 Cooley-Tukey FFT
    void fftInPlace(float* re, float* im, int n, bool inverse = false) {
        // Bit-reversal permutation
        for (int i = 1, j = 0; i < n; ++i) {
            int bit = n >> 1;
            while (j & bit) {
                j ^= bit;
                bit >>= 1;
            }
            j ^= bit;
            if (i < j) {
                std::swap(re[i], re[j]);
                std::swap(im[i], im[j]);
            }
        }

        // Cooley-Tukey iterative FFT
        for (int len = 2; len <= n; len <<= 1) {
            float angle = (inverse ? 2.0f : -2.0f) * PI / len;
            float wRe = std::cos(angle);
            float wIm = std::sin(angle);

            for (int i = 0; i < n; i += len) {
                float uRe = 1.0f, uIm = 0.0f;
                for (int j = 0; j < len / 2; ++j) {
                    int a = i + j;
                    int b = i + j + len / 2;

                    float tRe = re[b] * uRe - im[b] * uIm;
                    float tIm = re[b] * uIm + im[b] * uRe;

                    re[b] = re[a] - tRe;
                    im[b] = im[a] - tIm;
                    re[a] = re[a] + tRe;
                    im[a] = im[a] + tIm;

                    float newURe = uRe * wRe - uIm * wIm;
                    uIm = uRe * wIm + uIm * wRe;
                    uRe = newURe;
                }
            }
        }

        // Scale for inverse FFT
        if (inverse) {
            float scale = 1.0f / n;
            for (int i = 0; i < n; ++i) {
                re[i] *= scale;
                im[i] *= scale;
            }
        }
    }

    // Process one block of FFT_SIZE samples for SSB demodulation
    void processBlock() {
        // Apply window and copy to FFT buffers
        for (int n = 0; n < FFT_SIZE; ++n) {
            fftRe_[n] = inputI_[n] * window_[n];
            fftIm_[n] = inputQ_[n] * window_[n];
        }

        // Forward FFT
        fftInPlace(fftRe_.data(), fftIm_.data(), FFT_SIZE, false);

        // Zero out unwanted sideband for perfect image rejection
        // FFT output: bin 0 = DC, bins 1..N/2-1 = positive freq, bin N/2 = Nyquist,
        //             bins N/2+1..N-1 = negative freq
        if (mode_ == Mode::USB) {
            // Keep positive frequencies (bins 1 to N/2), zero negative (bins N/2+1 to N-1)
            // DC and Nyquist are real-only, keep them at half amplitude
            fftRe_[0] *= 0.5f;
            fftIm_[0] = 0.0f;
            fftRe_[FFT_SIZE / 2] *= 0.5f;
            fftIm_[FFT_SIZE / 2] = 0.0f;
            for (int k = FFT_SIZE / 2 + 1; k < FFT_SIZE; ++k) {
                fftRe_[k] = 0.0f;
                fftIm_[k] = 0.0f;
            }
        } else {  // LSB
            // Keep negative frequencies (bins N/2+1 to N-1), zero positive (bins 1 to N/2-1)
            fftRe_[0] *= 0.5f;
            fftIm_[0] = 0.0f;
            for (int k = 1; k < FFT_SIZE / 2; ++k) {
                fftRe_[k] = 0.0f;
                fftIm_[k] = 0.0f;
            }
            fftRe_[FFT_SIZE / 2] *= 0.5f;
            fftIm_[FFT_SIZE / 2] = 0.0f;
        }

        // Inverse FFT
        fftInPlace(fftRe_.data(), fftIm_.data(), FFT_SIZE, true);

        // Power calculation from kept bins
        float totalPower = 0.0f;
        if (mode_ == Mode::USB) {
            totalPower += fftRe_[0]*fftRe_[0]; // DC bin (already scaled)
            totalPower += fftRe_[FFT_SIZE/2]*fftRe_[FFT_SIZE/2]; // Nyquist
            for (int k = 1; k < FFT_SIZE / 2; ++k) {
                totalPower += 2.0f * (fftRe_[k]*fftRe_[k] + fftIm_[k]*fftIm_[k]);
            }
        } else {
            totalPower += fftRe_[0]*fftRe_[0];
            totalPower += fftRe_[FFT_SIZE/2]*fftRe_[FFT_SIZE/2];
            for (int k = FFT_SIZE / 2 + 1; k < FFT_SIZE; ++k) {
                totalPower += 2.0f * (fftRe_[k]*fftRe_[k] + fftIm_[k]*fftIm_[k]);
            }
        }
        // Correct for window gain: Hann window coherent gain is 0.5, power gain is 0.375
        // We multiply by sqrt(1/0.375) ≈ 1.633 to restore original power.
        updateRms(std::sqrt(totalPower / FFT_SIZE) * 1.633f, HOP_SIZE);

        // Overlap-add: take real part (audio)
        // Output = previous overlap + new first half (scaled by 2 since we zeroed half spectrum)
        for (int n = 0; n < HOP_SIZE; ++n) {
            outputBuffer_[n] = overlapBuffer_[n] + fftRe_[n] * 2.0f;
        }
        // Store second half for next overlap
        for (int n = 0; n < HOP_SIZE; ++n) {
            overlapBuffer_[n] = fftRe_[n + HOP_SIZE] * 2.0f;
        }
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
    std::array<BiquadState, LP_STAGES> amStateI_;
    std::array<BiquadState, LP_STAGES> amStateQ_;

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

        // 3kHz lowpass (4th order)
        constexpr float fc = 3000.0f;
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
