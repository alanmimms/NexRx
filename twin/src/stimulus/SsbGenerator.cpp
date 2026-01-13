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
    initPhaseIncrement();
}

void SsbGenerator::initPhaseIncrement() {
    // Precompute phase rotation for 480kHz oversample rate
    double delta = 2.0 * M_PI * carrier_hz_ / OVERSAMPLE_RATE;
    phaseDeltaCos_ = std::cos(delta);
    phaseDeltaSin_ = std::sin(delta);
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

void SsbGenerator::setAudioSamples(std::vector<float> samples, double sample_rate, bool repeat) {
    audioSource_ = AudioSource::Samples;
    samplesRepeat_ = repeat;

    // Resample to internal rate (48kHz) at load time with proper anti-alias filtering
    // This moves all the expensive work out of the real-time path
    resampleToInternalRate(samples, sample_rate);

    // Pre-compute Hilbert transform on the resampled audio
    precomputeHilbert();

    // Apply crossfade at loop point to prevent transients when audio repeats
    // This smooths any discontinuity between end and start of the buffer
    if (samplesRepeat_ && !audioSamples_.empty()) {
        applyLoopCrossfade();
    }

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

    // Pre-compute Hilbert transform of entire audio buffer
    // This converts O(65) per sample at runtime to O(1) lookup
    audioSamplesQ_.resize(audioSamples_.size());
    int center = HILBERT_TAPS / 2;
    size_t n = audioSamples_.size();

    for (size_t i = 0; i < n; ++i) {
        double result = 0.0;
        for (size_t k = 0; k < HILBERT_TAPS; ++k) {
            int offset = static_cast<int>(k) - center;
            // Use CONVOLUTION (i - offset), not correlation (i + offset)
            // Hilbert transform is h * x, not h ⋆ x
            int idx = static_cast<int>(i) - offset;

            // Handle wrap-around or zero-pad
            float sample = 0.0f;
            if (idx >= 0 && idx < static_cast<int>(n)) {
                sample = audioSamples_[idx];
            } else if (samplesRepeat_) {
                // Wrap around for repeating audio
                idx = ((idx % static_cast<int>(n)) + static_cast<int>(n)) % static_cast<int>(n);
                sample = audioSamples_[idx];
            }
            result += hilbertCoeffs_[k] * sample;
        }
        audioSamplesQ_[i] = static_cast<float>(result);
    }
}

void SsbGenerator::resampleToInternalRate(const std::vector<float>& input, double inputRate) {
    // Resample audio to internal rate (48kHz) using windowed-sinc interpolation
    // This provides proper anti-alias filtering during rate conversion.
    //
    // All resampling is done at load time - runtime just does simple lookups.

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

    // Calculate output size
    double ratio = INTERNAL_RATE / inputRate;
    size_t outputLen = static_cast<size_t>(input.size() * ratio + 0.5);
    audioSamples_.resize(outputLen);
    audioSampleRate_ = INTERNAL_RATE;

    // Windowed-sinc interpolation parameters
    // Filter half-width in input samples (larger = better quality, slower load)
    constexpr int FILTER_HALF = 16;

    // Cutoff at minimum of input/output Nyquist (with margin)
    double cutoff = 0.45 * std::min(1.0, 1.0 / ratio);

    // Precompute windowed-sinc coefficients for each output position's fractional offset
    // Use Kaiser window for good stopband attenuation
    auto kaiser = [](double n, double N, double beta) {
        if (N <= 0) return 1.0;
        double x = 2.0 * n / N - 1.0;
        if (std::abs(x) > 1.0) return 0.0;
        // I0(beta * sqrt(1 - x^2)) / I0(beta)
        auto bessel_i0 = [](double z) {
            double sum = 1.0, term = 1.0, z2 = z * z / 4.0;
            for (int k = 1; k < 25; ++k) {
                term *= z2 / (k * k);
                sum += term;
                if (term < 1e-12) break;
            }
            return sum;
        };
        double arg = beta * std::sqrt(1.0 - x * x);
        return bessel_i0(arg) / bessel_i0(beta);
    };

    // Resample each output sample
    for (size_t out = 0; out < outputLen; ++out) {
        double inPos = out / ratio;  // Position in input
        int inIdx = static_cast<int>(inPos);
        double frac = inPos - inIdx;

        double sum = 0.0;
        double weightSum = 0.0;

        for (int k = -FILTER_HALF; k <= FILTER_HALF; ++k) {
            int srcIdx = inIdx + k;

            // Handle boundaries
            float srcSample;
            if (srcIdx < 0) {
                srcSample = samplesRepeat_ ? input[(srcIdx % (int)input.size() + input.size()) % input.size()] : 0.0f;
            } else if (srcIdx >= (int)input.size()) {
                srcSample = samplesRepeat_ ? input[srcIdx % input.size()] : 0.0f;
            } else {
                srcSample = input[srcIdx];
            }

            // Sinc value at this offset
            double t = k - frac;
            double sinc = (std::abs(t) < 1e-9) ? 1.0 : std::sin(M_PI * t * 2 * cutoff) / (M_PI * t);

            // Kaiser window
            double window = kaiser(k + FILTER_HALF, 2 * FILTER_HALF, 8.0);

            double weight = sinc * window;
            sum += srcSample * weight;
            weightSum += weight;
        }

        audioSamples_[out] = static_cast<float>(weightSum > 0 ? sum / weightSum : 0.0);
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
            // For tones, Hilbert transform is exact: sin → -cos (90° lag)
            i = 0.0;
            q = 0.0;
            for (const auto& tone : tones_) {
                double phase = 2.0 * M_PI * tone.freq_hz * time_s;
                i += tone.amplitude * std::sin(phase);
                q += tone.amplitude * (-std::cos(phase));  // Hilbert of sin = -cos
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

            // Calculate sample index
            double sampleTime = time_s * audioSampleRate_;
            double duration = audioSamples_.size() / audioSampleRate_;

            if (samplesRepeat_) {
                sampleTime = std::fmod(sampleTime, static_cast<double>(audioSamples_.size()));
                if (sampleTime < 0) sampleTime += audioSamples_.size();
            } else if (time_s >= duration) {
                i = q = 0.0;
                break;
            }

            // Linear interpolation for both I and Q (pre-computed)
            size_t idx0 = static_cast<size_t>(sampleTime);
            size_t idx1 = (idx0 + 1) % audioSamples_.size();
            double frac = sampleTime - idx0;

            i = audioSamples_[idx0] * (1.0 - frac) + audioSamples_[idx1] * frac;
            // Use pre-computed Hilbert transform - O(1) instead of O(65)
            q = audioSamplesQ_[idx0] * (1.0 - frac) + audioSamplesQ_[idx1] * frac;
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

    if (tts_) {
        tts_->reset();
    }
}

void SsbGenerator::getRfIQ(double time_s, double& out_i, double& out_q) const {
    if (audioSource_ == AudioSource::None) {
        out_i = out_q = 0.0;
        return;
    }

    // Get audio I/Q (I = audio, Q = Hilbert(audio))
    double audioI, audioQ;
    getAudioIQ(time_s, audioI, audioQ);

    // Generate RF at carrier frequency using precomputed phase increment
    // This is O(1) per sample - just 4 muls + 2 adds, no trig calls
    if (!phaseInitialized_) {
        // Initialize phase on first call
        double phase = 2.0 * M_PI * carrier_hz_ * time_s;
        carrierCos_ = std::cos(phase);
        carrierSin_ = std::sin(phase);
        phaseInitialized_ = true;
    } else {
        // Incremental rotation using precomputed delta
        double new_cos = carrierCos_ * phaseDeltaCos_ - carrierSin_ * phaseDeltaSin_;
        double new_sin = carrierSin_ * phaseDeltaCos_ + carrierCos_ * phaseDeltaSin_;
        carrierCos_ = new_cos;
        carrierSin_ = new_sin;
    }

    double cosPhase = carrierCos_;
    double sinPhase = carrierSin_;

    // SSB modulates audio onto carrier using phasing method:
    // Audio analytic signal: z = audioI + j*audioQ (where audioQ = Hilbert(audioI))
    //
    // USB: RF = z * exp(j*ωc*t) = (audioI + j*audioQ) * exp(j*ωc*t)
    //      This produces signal at (fc + fa) for audio frequency fa
    // LSB: RF = z* * exp(j*ωc*t) = (audioI - j*audioQ) * exp(j*ωc*t)
    //      This produces signal at (fc - fa) for audio frequency fa
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
