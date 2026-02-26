// NexRx Digital Twin - AM Signal Generator Implementation
//
// Copyright 2026 NexRx Project - MIT License

#include "AmGenerator.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>

namespace nexrx {

AmGenerator::AmGenerator(double carrier_hz, double amplitude_v)
    : carrier_hz_(carrier_hz)
    , amplitude_v_(amplitude_v)
{
}

AmGenerator::~AmGenerator() = default;

void AmGenerator::setTones(const std::vector<double>& audio_freqs_hz) {
    audioSource_ = AudioSource::Tones;
    tones_.clear();
    
    if (audio_freqs_hz.empty()) return;

    // Distribute amplitude equally among tones
    double amp_per_tone = 1.0 / audio_freqs_hz.size();
    for (double f : audio_freqs_hz) {
        tones_.push_back({f, amp_per_tone});
    }
}

void AmGenerator::setAudioSamples(std::vector<float> samples, double sample_rate, bool repeat) {
    audioSource_ = AudioSource::Samples;
    audioSamples_ = std::move(samples);
    audioSampleRate_ = sample_rate;
    samplesRepeat_ = repeat;
}

double AmGenerator::getModulation(double time_s) const {
    switch (audioSource_) {
        case AudioSource::Tones: {
            double sum = 0.0;
            for (const auto& tone : tones_) {
                sum += tone.amplitude * std::cos(2.0 * M_PI * tone.freq_hz * time_s);
            }
            return sum;
        }

        case AudioSource::Samples: {
            if (audioSamples_.empty()) return 0.0;
            
            double sample_idx_f = time_s * audioSampleRate_;
            size_t total_samples = audioSamples_.size();

            if (samplesRepeat_) {
                sample_idx_f = std::fmod(sample_idx_f, static_cast<double>(total_samples));
                if (sample_idx_f < 0) sample_idx_f += total_samples;
            } else if (sample_idx_f >= total_samples || sample_idx_f < 0) {
                return 0.0;
            }

            size_t idx0 = static_cast<size_t>(sample_idx_f);
            size_t idx1 = (idx0 + 1) % total_samples;
            double frac = sample_idx_f - idx0;

            return audioSamples_[idx0] * (1.0 - frac) + audioSamples_[idx1] * frac;
        }

        default:
            return 0.0;
    }
}

double AmGenerator::getSample(double time_s) const {
    double mod = getModulation(time_s);
    double carrier = std::cos(2.0 * M_PI * carrier_hz_ * time_s);
    return amplitude_v_ * (1.0 + modIndex_ * mod) * carrier;
}

void AmGenerator::getRfIQ(double time_s, double& out_i, double& out_q) const {
    // Detect backward time jump or first call
    if (time_s < lastTime_ || lastTime_ < 0) {
        carrierPhase_ = std::fmod(2.0 * M_PI * carrier_hz_ * time_s, 2.0 * M_PI);
    } else {
        double dt = time_s - lastTime_;
        carrierPhase_ = std::fmod(carrierPhase_ + 2.0 * M_PI * carrier_hz_ * dt, 2.0 * M_PI);
    }
    lastTime_ = time_s;

    double mod = getModulation(time_s);
    double env = amplitude_v_ * (1.0 + modIndex_ * mod);

    out_i = env * std::cos(carrierPhase_);
    out_q = env * std::sin(carrierPhase_);
}

std::string AmGenerator::description() const {
    std::ostringstream oss;
    oss << "AM[" << carrier_hz_ / 1e6 << "MHz, mod=" << (int)(modIndex_ * 100) << "%, ";
    switch (audioSource_) {
        case AudioSource::Tones: oss << tones_.size() << " tone(s)"; break;
        case AudioSource::Samples: oss << "samples"; break;
        default: oss << "none"; break;
    }
    oss << "]";
    return oss.str();
}

void AmGenerator::reset() {
    lastTime_ = -1.0;
    carrierPhase_ = 0.0;
}

} // namespace nexrx
