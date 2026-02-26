// NexRx Digital Twin - SSB Signal Generator Implementation
//
// Copyright 2026 NexRx Project - MIT License

#include "SsbGenerator.hpp"
#include "TtsEngine.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>

namespace nexrx {

SsbGenerator::SsbGenerator(double carrier_hz, double amplitude_v, Mode mode)
    : carrier_hz_(carrier_hz)
    , amplitude_v_(amplitude_v)
    , mode_(mode)
{
    initHilbertFilter();
}

SsbGenerator::~SsbGenerator() = default;

void SsbGenerator::initHilbertFilter() {
    // Design FIR Hilbert transformer using windowed sinc
    // Odd-length filter with zero at DC and Nyquist
    hilbertCoeffs_.resize(HILBERT_TAPS);
    hilbertHistory_.resize(HILBERT_TAPS, 0.0);

    int center = HILBERT_TAPS / 2;

    for (size_t i = 0; i < HILBERT_TAPS; ++i) {
        int n = static_cast<int>(i) - center;
        if (n == 0) {
            hilbertCoeffs_[i] = 0.0;  // Zero at center
        } else if (n % 2 == 0) {
            hilbertCoeffs_[i] = 0.0;  // Zero at even samples
        } else {
            // Hilbert: h[n] = 2/(pi*n) for odd n
            hilbertCoeffs_[i] = 2.0 / (M_PI * n);
        }

        // Apply Blackman window
        double w = 0.42 - 0.5 * std::cos(2.0 * M_PI * i / (HILBERT_TAPS - 1))
                       + 0.08 * std::cos(4.0 * M_PI * i / (HILBERT_TAPS - 1));
        hilbertCoeffs_[i] *= w;
    }
}

void SsbGenerator::setTones(const std::vector<double>& audio_freqs_hz) {
    audioSource_ = AudioSource::Tones;
    tones_.clear();

    if (audio_freqs_hz.empty()) {
        return;
    }

    // Equal amplitude for all tones, normalized so sum = 1
    double amp = 1.0 / audio_freqs_hz.size();
    for (double freq : audio_freqs_hz) {
        tones_.push_back({freq, amp});
    }
}

void SsbGenerator::setVoice(std::shared_ptr<TtsEngine> tts, bool repeat) {
    audioSource_ = AudioSource::Voice;
    tts_ = std::move(tts);
    voiceRepeat_ = repeat;
}

// Two-stage upsampling: 2x FFT (exact, no images) + 5x polyphase sinc
// This avoids the power-of-2 ratio problem while staying fast
static std::vector<float> upsampleBuffer(const std::vector<float>& input, double inputRate, double outputRate, bool repeat) {
    if (input.empty() || inputRate <= 0 || outputRate <= inputRate) {
        return input;
    }

    // For 48kHz → 480kHz (10x), do 2x FFT then 5x polyphase
    // Stage 1: 2x upsample using FFT (perfect, fast)
    size_t inputLen = input.size();
    size_t stage1Len = inputLen * 2;

    // Find power-of-2 FFT size for input
    size_t fftSize = 1;
    while (fftSize < inputLen) fftSize <<= 1;

    std::vector<double> re(fftSize * 2, 0.0);
    std::vector<double> im(fftSize * 2, 0.0);

    // Copy input
    for (size_t i = 0; i < inputLen; ++i) {
        re[i] = input[i];
    }

    // In-place radix-2 FFT
    auto fftInPlace = [](double* re, double* im, size_t n, bool inverse) {
        for (size_t i = 1, j = 0; i < n; ++i) {
            size_t bit = n >> 1;
            while (j & bit) { j ^= bit; bit >>= 1; }
            j ^= bit;
            if (i < j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); }
        }
        for (size_t len = 2; len <= n; len <<= 1) {
            double angle = (inverse ? 2.0 : -2.0) * M_PI / len;
            double wRe = std::cos(angle), wIm = std::sin(angle);
            for (size_t i = 0; i < n; i += len) {
                double uRe = 1.0, uIm = 0.0;
                for (size_t j = 0; j < len / 2; ++j) {
                    size_t a = i + j, b = i + j + len / 2;
                    double tRe = re[b] * uRe - im[b] * uIm;
                    double tIm = re[b] * uIm + im[b] * uRe;
                    re[b] = re[a] - tRe; im[b] = im[a] - tIm;
                    re[a] += tRe; im[a] += tIm;
                    double newURe = uRe * wRe - uIm * wIm;
                    uIm = uRe * wIm + uIm * wRe; uRe = newURe;
                }
            }
        }
        if (inverse) {
            double scale = 1.0 / n;
            for (size_t i = 0; i < n; ++i) { re[i] *= scale; im[i] *= scale; }
        }
    };

    // Forward FFT
    fftInPlace(re.data(), im.data(), fftSize, false);

    // Zero-insert for 2x: move negative freqs to new positions
    std::vector<double> re2(fftSize * 2, 0.0);
    std::vector<double> im2(fftSize * 2, 0.0);
    // DC and positive frequencies
    for (size_t k = 0; k <= fftSize / 2; ++k) {
        re2[k] = re[k];
        im2[k] = im[k];
    }
    // Negative frequencies
    for (size_t k = fftSize / 2 + 1; k < fftSize; ++k) {
        re2[fftSize * 2 - (fftSize - k)] = re[k];
        im2[fftSize * 2 - (fftSize - k)] = im[k];
    }

    // Inverse FFT at 2x size
    fftInPlace(re2.data(), im2.data(), fftSize * 2, true);

    // Extract 2x upsampled result
    std::vector<float> stage1(stage1Len);
    for (size_t i = 0; i < stage1Len; ++i) {
        stage1[i] = static_cast<float>(re2[i] * 2.0);  // Scale by 2 for amplitude
    }

    // Stage 2: 5x polyphase upsampling
    // Precompute filter coefficients for all 5 phases
    const int UPSAMPLE = 5;
    const int FILTER_HALF = 12;
    const int TAPS = 2 * FILTER_HALF + 1;  // 25 taps per phase
    const double KAISER_BETA = 8.0;
    const double CUTOFF = 0.9 / UPSAMPLE;  // 0.18

    // Precompute Kaiser-windowed sinc for all phases
    // polyphase[phase][tap] = filter coefficient
    std::vector<std::vector<double>> polyphase(UPSAMPLE, std::vector<double>(TAPS));
    std::vector<double> phaseNorm(UPSAMPLE);  // Normalization per phase

    auto bessel_i0 = [](double z) {
        double sum = 1.0, term = 1.0, z2 = z * z / 4.0;
        for (int k = 1; k < 25; ++k) {
            term *= z2 / (k * k);
            sum += term;
            if (term < 1e-12) break;
        }
        return sum;
    };
    double i0_beta = bessel_i0(KAISER_BETA);

    for (int phase = 0; phase < UPSAMPLE; ++phase) {
        double frac = phase / static_cast<double>(UPSAMPLE);
        double weightSum = 0.0;
        for (int tap = 0; tap < TAPS; ++tap) {
            int k = tap - FILTER_HALF;
            double t = k - frac;
            double sinc = (std::abs(t) < 1e-9) ? 1.0 : std::sin(M_PI * t * 2 * CUTOFF) / (M_PI * t);
            // Kaiser window
            double x = 2.0 * tap / (TAPS - 1) - 1.0;
            double window = bessel_i0(KAISER_BETA * std::sqrt(std::max(0.0, 1.0 - x * x))) / i0_beta;
            polyphase[phase][tap] = sinc * window;
            weightSum += polyphase[phase][tap];
        }
        phaseNorm[phase] = (weightSum > 0) ? 1.0 / weightSum : 0.0;
    }

    // Apply polyphase filter
    size_t outputLen = stage1Len * UPSAMPLE;
    std::vector<float> output(outputLen);
    int stage1Size = static_cast<int>(stage1.size());

    for (size_t in = 0; in < stage1Len; ++in) {
        for (int phase = 0; phase < UPSAMPLE; ++phase) {
            double sum = 0.0;
            const auto& coeffs = polyphase[phase];
            for (int tap = 0; tap < TAPS; ++tap) {
                int srcIdx = static_cast<int>(in) + tap - FILTER_HALF;
                float srcSample;
                if (srcIdx < 0 || srcIdx >= stage1Size) {
                    if (repeat) {
                        srcIdx = ((srcIdx % stage1Size) + stage1Size) % stage1Size;
                        srcSample = stage1[srcIdx];
                    } else {
                        srcSample = 0.0f;
                    }
                } else {
                    srcSample = stage1[srcIdx];
                }
                sum += srcSample * coeffs[tap];
            }
            output[in * UPSAMPLE + phase] = static_cast<float>(sum * phaseNorm[phase]);
        }
    }

    return output;
}

void SsbGenerator::setAudioSamples(std::vector<float> samples, double sample_rate, bool repeat) {
    audioSource_ = AudioSource::Samples;
    samplesRepeat_ = repeat;

    // Step 1: Resample to 48kHz for fast Hilbert computation
    resampleToInternalRate(samples, sample_rate);

    // Step 2: Compute Hilbert transform at 48kHz (fast FFT on ~70k samples)
    precomputeHilbert();

    // Step 3: Apply crossfade at loop point (before upsampling)
    if (samplesRepeat_ && !audioSamples_.empty()) {
        applyLoopCrossfade();
    }

    // Step 4: Upsample BOTH I and Q to 480kHz with proper anti-alias filtering
    // This eliminates runtime interpolation artifacts entirely
    audioSamples_ = upsampleBuffer(audioSamples_, 48000.0, 480000.0, samplesRepeat_);
    audioSamplesQ_ = upsampleBuffer(audioSamplesQ_, 48000.0, 480000.0, samplesRepeat_);
    audioSampleRate_ = 480000.0;

    // Reset Hilbert filter state (still used for voice)
    std::fill(hilbertHistory_.begin(), hilbertHistory_.end(), 0.0);
    hilbertIndex_ = 0;
    lastSampleTime_ = -1.0;
}

void SsbGenerator::precomputeHilbert() {
    if (audioSamples_.empty()) {
        audioSamplesQ_.clear();
        return;
    }

    // FFT-based Hilbert transform - O(n log n) instead of O(n * taps)
    // Critical for large buffers (4M+ samples at 480kHz)

    size_t n = audioSamples_.size();

    // Find next power of 2
    size_t fftSize = 1;
    while (fftSize < n) fftSize <<= 1;

    std::vector<double> re(fftSize, 0.0);
    std::vector<double> im(fftSize, 0.0);

    // Copy audio - for looping audio, use circular extension to avoid edge artifacts
    for (size_t i = 0; i < fftSize; ++i) {
        re[i] = (i < n) ? audioSamples_[i] : (samplesRepeat_ ? audioSamples_[i % n] : 0.0);
    }

    // In-place radix-2 FFT
    auto fftInPlace = [](double* re, double* im, size_t n, bool inverse) {
        for (size_t i = 1, j = 0; i < n; ++i) {
            size_t bit = n >> 1;
            while (j & bit) { j ^= bit; bit >>= 1; }
            j ^= bit;
            if (i < j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); }
        }
        for (size_t len = 2; len <= n; len <<= 1) {
            double angle = (inverse ? 2.0 : -2.0) * M_PI / len;
            double wRe = std::cos(angle), wIm = std::sin(angle);
            for (size_t i = 0; i < n; i += len) {
                double uRe = 1.0, uIm = 0.0;
                for (size_t j = 0; j < len / 2; ++j) {
                    size_t a = i + j, b = i + j + len / 2;
                    double tRe = re[b] * uRe - im[b] * uIm;
                    double tIm = re[b] * uIm + im[b] * uRe;
                    re[b] = re[a] - tRe; im[b] = im[a] - tIm;
                    re[a] += tRe; im[a] += tIm;
                    double newURe = uRe * wRe - uIm * wIm;
                    uIm = uRe * wIm + uIm * wRe; uRe = newURe;
                }
            }
        }
        if (inverse) {
            double scale = 1.0 / n;
            for (size_t i = 0; i < n; ++i) { re[i] *= scale; im[i] *= scale; }
        }
    };

    // Forward FFT
    fftInPlace(re.data(), im.data(), fftSize, false);

    // Create analytic signal: double positive freq, zero negative freq
    for (size_t k = 1; k < fftSize / 2; ++k) { re[k] *= 2.0; im[k] *= 2.0; }
    for (size_t k = fftSize / 2 + 1; k < fftSize; ++k) { re[k] = 0.0; im[k] = 0.0; }

    // Inverse FFT
    fftInPlace(re.data(), im.data(), fftSize, true);

    // Extract Hilbert transform (imaginary part)
    audioSamplesQ_.resize(n);
    for (size_t i = 0; i < n; ++i) {
        audioSamplesQ_[i] = static_cast<float>(im[i]);
    }
}

void SsbGenerator::resampleToInternalRate(const std::vector<float>& input, double inputRate) {
    // Resample audio to 48kHz using two-stage approach for clean resampling
    // 8kHz → 48kHz is 6x = 2x (FFT) * 3x (polyphase)

    constexpr double INTERNAL_RATE = 48000.0;

    if (input.empty() || inputRate <= 0) {
        audioSamples_.clear();
        audioSampleRate_ = INTERNAL_RATE;
        return;
    }

    // If already at internal rate, just copy
    if (std::abs(inputRate - INTERNAL_RATE) < 1.0) {
        audioSamples_ = input;
        audioSampleRate_ = INTERNAL_RATE;
        return;
    }

    double ratio = INTERNAL_RATE / inputRate;
    audioSampleRate_ = INTERNAL_RATE;

    // In-place radix-2 FFT helper
    auto fftInPlace = [](double* re, double* im, size_t n, bool inverse) {
        for (size_t i = 1, j = 0; i < n; ++i) {
            size_t bit = n >> 1;
            while (j & bit) { j ^= bit; bit >>= 1; }
            j ^= bit;
            if (i < j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); }
        }
        for (size_t len = 2; len <= n; len <<= 1) {
            double angle = (inverse ? 2.0 : -2.0) * M_PI / len;
            double wRe = std::cos(angle), wIm = std::sin(angle);
            for (size_t i = 0; i < n; i += len) {
                double uRe = 1.0, uIm = 0.0;
                for (size_t j = 0; j < len / 2; ++j) {
                    size_t a = i + j, b = i + j + len / 2;
                    double tRe = re[b] * uRe - im[b] * uIm;
                    double tIm = re[b] * uIm + im[b] * uRe;
                    re[b] = re[a] - tRe; im[b] = im[a] - tIm;
                    re[a] += tRe; im[a] += tIm;
                    double newURe = uRe * wRe - uIm * wIm;
                    uIm = uRe * wIm + uIm * wRe; uRe = newURe;
                }
            }
        }
        if (inverse) {
            double scale = 1.0 / n;
            for (size_t i = 0; i < n; ++i) { re[i] *= scale; im[i] *= scale; }
        }
    };

    // For 6x (8kHz→48kHz): do 2x FFT then 3x polyphase
    // For other ratios: use direct polyphase with corrected cutoff

    size_t inputLen = input.size();
    std::vector<float> current = input;

    // Stage 1: 2x FFT upsampling (if ratio >= 2)
    if (ratio >= 2.0) {
        size_t stage1Len = inputLen * 2;
        size_t fftSize = 1;
        while (fftSize < inputLen) fftSize <<= 1;

        std::vector<double> re(fftSize * 2, 0.0);
        std::vector<double> im(fftSize * 2, 0.0);
        for (size_t i = 0; i < inputLen; ++i) re[i] = current[i];

        fftInPlace(re.data(), im.data(), fftSize, false);

        std::vector<double> re2(fftSize * 2, 0.0);
        std::vector<double> im2(fftSize * 2, 0.0);
        for (size_t k = 0; k <= fftSize / 2; ++k) { re2[k] = re[k]; im2[k] = im[k]; }
        for (size_t k = fftSize / 2 + 1; k < fftSize; ++k) {
            re2[fftSize * 2 - (fftSize - k)] = re[k];
            im2[fftSize * 2 - (fftSize - k)] = im[k];
        }

        fftInPlace(re2.data(), im2.data(), fftSize * 2, true);

        current.resize(stage1Len);
        for (size_t i = 0; i < stage1Len; ++i) {
            current[i] = static_cast<float>(re2[i] * 2.0);
        }
        inputLen = stage1Len;
        ratio /= 2.0;
    }

    // Stage 2: Remaining ratio using polyphase (e.g., 3x for 8kHz→48kHz)
    if (ratio > 1.01) {
        int UPSAMPLE = static_cast<int>(ratio + 0.5);
        const int FILTER_HALF = 12;
        const int TAPS = 2 * FILTER_HALF + 1;
        const double KAISER_BETA = 8.0;
        const double CUTOFF = 0.45;  // Pass 90% of input bandwidth

        auto bessel_i0 = [](double z) {
            double sum = 1.0, term = 1.0, z2 = z * z / 4.0;
            for (int k = 1; k < 25; ++k) {
                term *= z2 / (k * k);
                sum += term;
                if (term < 1e-12) break;
            }
            return sum;
        };
        double i0_beta = bessel_i0(KAISER_BETA);

        // Precompute polyphase filter coefficients
        std::vector<std::vector<double>> polyphase(UPSAMPLE, std::vector<double>(TAPS));
        std::vector<double> phaseNorm(UPSAMPLE);

        for (int phase = 0; phase < UPSAMPLE; ++phase) {
            double frac = phase / static_cast<double>(UPSAMPLE);
            double weightSum = 0.0;
            for (int tap = 0; tap < TAPS; ++tap) {
                int k = tap - FILTER_HALF;
                double t = k - frac;
                double sinc = (std::abs(t) < 1e-9) ? 1.0 : std::sin(M_PI * t * 2 * CUTOFF) / (M_PI * t);
                double x = 2.0 * tap / (TAPS - 1) - 1.0;
                double window = bessel_i0(KAISER_BETA * std::sqrt(std::max(0.0, 1.0 - x * x))) / i0_beta;
                polyphase[phase][tap] = sinc * window;
                weightSum += polyphase[phase][tap];
            }
            phaseNorm[phase] = (weightSum > 0) ? 1.0 / weightSum : 0.0;
        }

        size_t outputLen = inputLen * UPSAMPLE;
        audioSamples_.resize(outputLen);
        int currentSize = static_cast<int>(current.size());

        for (size_t in = 0; in < inputLen; ++in) {
            for (int phase = 0; phase < UPSAMPLE; ++phase) {
                double sum = 0.0;
                const auto& coeffs = polyphase[phase];
                for (int tap = 0; tap < TAPS; ++tap) {
                    int srcIdx = static_cast<int>(in) + tap - FILTER_HALF;
                    float srcSample;
                    if (srcIdx < 0 || srcIdx >= currentSize) {
                        if (samplesRepeat_) {
                            srcIdx = ((srcIdx % currentSize) + currentSize) % currentSize;
                            srcSample = current[srcIdx];
                        } else {
                            srcSample = 0.0f;
                        }
                    } else {
                        srcSample = current[srcIdx];
                    }
                    sum += srcSample * coeffs[tap];
                }
                audioSamples_[in * UPSAMPLE + phase] = static_cast<float>(sum * phaseNorm[phase]);
            }
        }
    } else {
        audioSamples_ = current;
    }
}

void SsbGenerator::applyLoopCrossfade() {
    // Apply crossfade at loop boundary to eliminate discontinuity transients.
    // Without this, the jump from end-of-buffer to start-of-buffer creates
    // broadband impulse noise that appears as spikes near Nyquist.
    //
    // We blend the last N samples with the first N samples using a raised
    // cosine fade curve, applied to both I and Q channels.

    if (audioSamples_.empty()) return;

    // Crossfade length: ~20ms at 48kHz = 960 samples
    // Long enough to avoid creating new transients, short enough to preserve audio
    constexpr size_t CROSSFADE_SAMPLES = 960;
    size_t fadeLen = std::min(CROSSFADE_SAMPLES, audioSamples_.size() / 4);

    if (fadeLen < 2) return;

    size_t n = audioSamples_.size();

    // Apply crossfade to I channel (audioSamples_)
    // The last fadeLen samples fade from their original values toward sample[0].
    // This ensures audioSamples_[N-1] ≈ audioSamples_[0] for seamless wrap.
    float target_i = audioSamples_[0];  // Target: first sample
    for (size_t i = 0; i < fadeLen; ++i) {
        double t = static_cast<double>(i) / (fadeLen - 1);  // 0 → 1
        double blend = 0.5 * (1.0 - std::cos(M_PI * t));    // 0 → 1 (raised cosine)

        size_t endIdx = n - fadeLen + i;
        float endVal = audioSamples_[endIdx];

        // Blend from original toward target (sample 0)
        audioSamples_[endIdx] = static_cast<float>(endVal * (1.0 - blend) + target_i * blend);
    }

    // Apply same crossfade to Q channel (audioSamplesQ_)
    if (audioSamplesQ_.size() == n) {
        float target_q = audioSamplesQ_[0];
        for (size_t i = 0; i < fadeLen; ++i) {
            double t = static_cast<double>(i) / (fadeLen - 1);
            double blend = 0.5 * (1.0 - std::cos(M_PI * t));

            size_t endIdx = n - fadeLen + i;
            float endVal = audioSamplesQ_[endIdx];

            audioSamplesQ_[endIdx] = static_cast<float>(endVal * (1.0 - blend) + target_q * blend);
        }
    }
}

void SsbGenerator::getAudioIQ(double time_s, double& i, double& q) const {
    switch (audioSource_) {
        case AudioSource::Tones: {
            i = 0.0;
            q = 0.0;
            for (const auto& tone : tones_) {
                double phase = 2.0 * M_PI * tone.freq_hz * time_s;
                i += tone.amplitude * std::cos(phase);
                q += tone.amplitude * std::sin(phase);
            }
            break;
        }

        case AudioSource::Voice: {
            if (tts_) {
                // Get audio from espeak-ng TTS engine
                i = tts_->getSample(time_s);
                // Use FIR Hilbert filter for voice
                q = hilbertFilter(time_s);
            } else {
                i = q = 0.0;
            }
            break;
        }

        case AudioSource::Samples: {
            if (audioSamples_.empty() || audioSampleRate_ <= 0) {
                i = q = 0.0;
                break;
            }

            // Audio is pre-upsampled to 480kHz - direct lookup, no interpolation needed
            double sampleTime = time_s * audioSampleRate_;
            size_t n = audioSamples_.size();

            if (samplesRepeat_) {
                sampleTime = std::fmod(sampleTime, static_cast<double>(n));
                if (sampleTime < 0) sampleTime += n;
            } else if (time_s >= n / audioSampleRate_) {
                i = q = 0.0;
                break;
            }

            size_t idx = static_cast<size_t>(sampleTime) % n;
            i = audioSamples_[idx];
            q = audioSamplesQ_[idx];
            break;
        }

        case AudioSource::None:
        default:
            i = q = 0.0;
            break;
    }
}

double SsbGenerator::hilbertFilter(double time_s) const {
    // This is a simplified approach - for accurate results with
    // arbitrary audio, we'd need to properly track sample timing.
    // For now, use the direct method for sample-based audio.

    if (audioSource_ == AudioSource::Samples && !audioSamples_.empty()) {
        // Apply FIR Hilbert filter to samples
        double sampleTime = time_s * audioSampleRate_;
        if (samplesRepeat_) {
            sampleTime = std::fmod(sampleTime, static_cast<double>(audioSamples_.size()));
        }

        double result = 0.0;
        int center = HILBERT_TAPS / 2;

        for (size_t i = 0; i < HILBERT_TAPS; ++i) {
            int offset = static_cast<int>(i) - center;
            double sampleIdx = sampleTime + offset;

            if (samplesRepeat_) {
                while (sampleIdx < 0) sampleIdx += audioSamples_.size();
                while (sampleIdx >= audioSamples_.size()) sampleIdx -= audioSamples_.size();
            } else {
                if (sampleIdx < 0 || sampleIdx >= audioSamples_.size()) continue;
            }

            size_t idx0 = static_cast<size_t>(sampleIdx);
            size_t idx1 = (idx0 + 1) % audioSamples_.size();
            double frac = sampleIdx - idx0;
            double sample = audioSamples_[idx0] * (1.0 - frac) + audioSamples_[idx1] * frac;

            result += hilbertCoeffs_[i] * sample;
        }
        return result;
    }

    // For espeak voice/TTS, get samples and filter
    if (audioSource_ == AudioSource::Voice && tts_) {
        // Approximate sample rate for TTS
        constexpr double TTS_SAMPLE_RATE = 22050.0;
        double result = 0.0;
        int center = HILBERT_TAPS / 2;

        for (size_t i = 0; i < HILBERT_TAPS; ++i) {
            int offset = static_cast<int>(i) - center;
            double sampleTime = time_s + offset / TTS_SAMPLE_RATE;
            double sample = tts_->getSample(sampleTime);
            result += hilbertCoeffs_[i] * sample;
        }
        return result;
    }

    return 0.0;
}

double SsbGenerator::getSample(double time_s) const {
    if (audioSource_ == AudioSource::None) {
        return 0.0;
    }

    // Get audio I/Q (I = audio, Q = Hilbert(audio))
    double audioI, audioQ;
    getAudioIQ(time_s, audioI, audioQ);

    // Generate carrier
    double carrierPhase = 2.0 * M_PI * carrier_hz_ * time_s;
    double cosCarrier = std::cos(carrierPhase);
    double sinCarrier = std::sin(carrierPhase);

    // SSB phasing method for REAL output signal
    // With audioI = audio and audioQ = Hilbert(audio):
    // USB: output = audioI*cos - audioQ*sin → signal at fc + fa
    // LSB: output = audioI*cos + audioQ*sin → signal at fc - fa
    //
    // Note: audioQ in tone mode is -cos(ωa*t), so for USB with audio = sin(ωa*t):
    //   output = sin(ωa)*cos(ωc) - (-cos(ωa))*sin(ωc)
    //          = sin(ωa)*cos(ωc) + cos(ωa)*sin(ωc) = sin(ωa + ωc) ✓
    double output;
    if (mode_ == Mode::USB) {
        output = audioI * cosCarrier - audioQ * sinCarrier;
    } else {
        output = audioI * cosCarrier + audioQ * sinCarrier;
    }

    return amplitude_v_ * output;
}

std::string SsbGenerator::description() const {
    std::ostringstream oss;
    oss << "SSB[" << carrier_hz_ / 1e6 << "MHz, ";
    oss << (mode_ == Mode::USB ? "USB" : "LSB") << ", ";

    switch (audioSource_) {
        case AudioSource::Tones:
            oss << tones_.size() << " tone(s)";
            break;
        case AudioSource::Voice:
            oss << "espeak";
            break;
        case AudioSource::Samples:
            oss << "samples";
            break;
        default:
            oss << "none";
            break;
    }
    oss << "]";
    return oss.str();
}

void SsbGenerator::reset() {
    std::fill(hilbertHistory_.begin(), hilbertHistory_.end(), 0.0);
    hilbertIndex_ = 0;
    lastSampleTime_ = -1.0;
    lastTime_ = -1.0;
    carrierPhase_ = 0.0;

    if (tts_) {
        tts_->reset();
    }
}

void SsbGenerator::getRfIQ(double time_s, double& out_i, double& out_q) const {
    if (audioSource_ == AudioSource::None) {
        out_i = out_q = 0.0;
        return;
    }

    // Accumulate carrier phase, detecting backward time jump or first call
    if (time_s < lastTime_ || lastTime_ < 0) {
        carrierPhase_ = std::fmod(2.0 * M_PI * carrier_hz_ * time_s, 2.0 * M_PI);
    } else {
        double dt = time_s - lastTime_;
        carrierPhase_ = std::fmod(carrierPhase_ + 2.0 * M_PI * carrier_hz_ * dt, 2.0 * M_PI);
    }
    lastTime_ = time_s;

    // Get audio I/Q
    double audioI, audioQ;
    getAudioIQ(time_s, audioI, audioQ);

    double cosPhase = std::cos(carrierPhase_);
    double sinPhase = std::sin(carrierPhase_);

    if (mode_ == Mode::USB) {
        // (audioI + j*audioQ) * (cos + j*sin) =
        // (audioI*cos - audioQ*sin) + j*(audioI*sin + audioQ*cos)
        out_i = amplitude_v_ * (audioI * cosPhase - audioQ * sinPhase);
        out_q = amplitude_v_ * (audioI * sinPhase + audioQ * cosPhase);
    } else {
        // (audioI - j*audioQ) * (cos + j*sin) =
        // (audioI*cos + audioQ*sin) + j*(audioI*sin - audioQ*cos)
        out_i = amplitude_v_ * (audioI * cosPhase + audioQ * sinPhase);
        out_q = amplitude_v_ * (audioI * sinPhase - audioQ * cosPhase);
    }
}

} // namespace nexrx
