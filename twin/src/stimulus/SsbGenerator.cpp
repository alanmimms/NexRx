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

void SsbGenerator::setAudioSamples(std::vector<float> samples, double sample_rate, bool repeat) {
    audioSource_ = AudioSource::Samples;
    audioSamples_ = std::move(samples);
    audioSampleRate_ = sample_rate;
    samplesRepeat_ = repeat;

    // Reset Hilbert filter state
    std::fill(hilbertHistory_.begin(), hilbertHistory_.end(), 0.0);
    hilbertIndex_ = 0;
    lastSampleTime_ = -1.0;
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

            // Linear interpolation
            size_t idx0 = static_cast<size_t>(sampleTime);
            size_t idx1 = (idx0 + 1) % audioSamples_.size();
            double frac = sampleTime - idx0;

            i = audioSamples_[idx0] * (1.0 - frac) + audioSamples_[idx1] * frac;
            q = hilbertFilter(time_s);
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

    // SSB phasing method
    // USB: output = I*cos(wt) - Q*sin(wt) = upper sideband only
    // LSB: output = I*cos(wt) + Q*sin(wt) = lower sideband only
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

void SsbGenerator::getBasebandIQ(double time_s, double lo_freq_hz,
                                  double& out_i, double& out_q) const {
    if (audioSource_ == AudioSource::None) {
        out_i = out_q = 0.0;
        return;
    }

    // Get audio I/Q (I = audio, Q = Hilbert(audio))
    double audioI, audioQ;
    getAudioIQ(time_s, audioI, audioQ);

    // Baseband frequency = carrier - LO
    double baseband_freq = carrier_hz_ - lo_freq_hz;
    double phase = 2.0 * M_PI * baseband_freq * time_s;
    double cosPhase = std::cos(phase);
    double sinPhase = std::sin(phase);

    // SSB shifts audio to baseband:
    // USB: baseband = (audioI - j*audioQ) * exp(j*phase)
    // LSB: baseband = (audioI + j*audioQ) * exp(j*phase)
    if (mode_ == Mode::USB) {
        // (audioI - j*audioQ) * (cos + j*sin) =
        // (audioI*cos + audioQ*sin) + j*(audioI*sin - audioQ*cos)
        out_i = amplitude_v_ * (audioI * cosPhase + audioQ * sinPhase);
        out_q = amplitude_v_ * (audioI * sinPhase - audioQ * cosPhase);
    } else {
        // (audioI + j*audioQ) * (cos + j*sin) =
        // (audioI*cos - audioQ*sin) + j*(audioI*sin + audioQ*cos)
        out_i = amplitude_v_ * (audioI * cosPhase - audioQ * sinPhase);
        out_q = amplitude_v_ * (audioI * sinPhase + audioQ * cosPhase);
    }
}

} // namespace nexrx
