// NexRx Digital Twin - Text-to-Speech Engine Implementation
//
// Copyright 2026 NexRx Project - MIT License

#include "TtsEngine.hpp"

#include <cmath>
#include <algorithm>

// Try to include espeak-ng if available
#if __has_include(<espeak-ng/speak_lib.h>)
#define HAS_ESPEAK_NG 1
#include <espeak-ng/speak_lib.h>
#include <espeak-ng/espeak_ng.h>
#else
#define HAS_ESPEAK_NG 0
#endif

namespace nexrx {

bool TtsEngine::initialized_ = false;

bool TtsEngine::initEspeak() {
#if HAS_ESPEAK_NG
    if (initialized_) return true;

    int result = espeak_Initialize(AUDIO_OUTPUT_SYNCHRONOUS, 0, nullptr, 0);
    if (result >= 0) {
        initialized_ = true;
        return true;
    }
    return false;
#else
    return false;
#endif
}

bool TtsEngine::isAvailable() {
#if HAS_ESPEAK_NG
    return initEspeak();
#else
    return false;
#endif
}

TtsEngine::TtsEngine() {
    initEspeak();
}

TtsEngine::~TtsEngine() {
    // Don't terminate espeak - it's shared
}

void TtsEngine::setText(const std::string& text) {
    if (text != text_) {
        text_ = text;
        needsSynthesize_ = true;
    }
}

void TtsEngine::setRate(int wpm) {
    if (wpm != rate_) {
        rate_ = std::clamp(wpm, 80, 450);
        needsSynthesize_ = true;
    }
}

void TtsEngine::setPitch(int pitch) {
    if (pitch != pitch_) {
        pitch_ = std::clamp(pitch, 0, 100);
        needsSynthesize_ = true;
    }
}

#if HAS_ESPEAK_NG
// Callback for espeak synthesis
static std::vector<float>* g_sampleBuffer = nullptr;

static int espeakCallback(short* wav, int numsamples, espeak_EVENT* events) {
    (void)events;
    if (wav && numsamples > 0 && g_sampleBuffer) {
        for (int i = 0; i < numsamples; ++i) {
            g_sampleBuffer->push_back(wav[i] / 32768.0f);
        }
    }
    return 0;  // Continue
}
#endif

void TtsEngine::synthesizeInternal() {
    if (!needsSynthesize_) return;
    needsSynthesize_ = false;
    samples_.clear();

    if (text_.empty()) return;

#if HAS_ESPEAK_NG
    if (initialized_) {
        // Configure espeak
        espeak_SetParameter(espeakRATE, rate_, 0);
        espeak_SetParameter(espeakPITCH, pitch_, 0);
        espeak_SetParameter(espeakVOLUME, 100, 0);

        // Set up callback
        g_sampleBuffer = &samples_;
        espeak_SetSynthCallback(espeakCallback);

        // Synthesize
        unsigned int flags = espeakCHARS_AUTO | espeakPHONEMES | espeakENDPAUSE;
        espeak_Synth(text_.c_str(), text_.size() + 1, 0, POS_CHARACTER, 0, flags, nullptr, nullptr);
        espeak_Synchronize();

        g_sampleBuffer = nullptr;

        // Get actual sample rate from espeak
        sampleRate_ = espeak_ng_GetSampleRate();
        if (sampleRate_ <= 0) sampleRate_ = 22050.0;

        return;
    }
#endif

    // Fallback: generate a simple beep pattern as placeholder
    // This allows testing without espeak-ng installed
    sampleRate_ = 22050.0;

    // Generate 1 second of 1kHz tone per 10 characters
    double duration = std::max(1.0, text_.size() / 10.0);
    size_t numSamples = static_cast<size_t>(duration * sampleRate_);
    samples_.resize(numSamples);

    for (size_t i = 0; i < numSamples; ++i) {
        double t = i / sampleRate_;
        // Simple envelope
        double env = 1.0;
        if (t < 0.05) env = t / 0.05;
        if (t > duration - 0.05) env = (duration - t) / 0.05;

        // 1kHz tone with slight frequency variation
        double freq = 1000.0 + 100.0 * std::sin(2.0 * M_PI * 0.5 * t);
        samples_[i] = static_cast<float>(env * 0.5 * std::sin(2.0 * M_PI * freq * t));
    }
}

std::vector<float> TtsEngine::synthesize(const std::string& text) {
    setText(text);
    synthesizeInternal();
    return samples_;
}

float TtsEngine::getSample(double time_s) const {
    // Ensure samples are ready (const_cast for lazy synthesis)
    const_cast<TtsEngine*>(this)->synthesizeInternal();

    if (samples_.empty()) return 0.0f;

    double duration = samples_.size() / sampleRate_;

    if (!repeat_ && time_s >= duration) {
        return 0.0f;
    }

    // Calculate sample position with repeat
    double effectiveTime = repeat_ ? std::fmod(time_s, duration) : time_s;
    if (effectiveTime < 0) effectiveTime += duration;

    double samplePos = effectiveTime * sampleRate_;
    size_t idx0 = static_cast<size_t>(samplePos);
    size_t idx1 = (idx0 + 1) % samples_.size();
    double frac = samplePos - idx0;

    // Linear interpolation
    return samples_[idx0] * (1.0f - static_cast<float>(frac))
         + samples_[idx1] * static_cast<float>(frac);
}

double TtsEngine::getDuration() const {
    const_cast<TtsEngine*>(this)->synthesizeInternal();
    if (samples_.empty() || sampleRate_ <= 0) return 0.0;
    return samples_.size() / sampleRate_;
}

void TtsEngine::reset() {
    // Nothing to reset - synthesis is time-indexed
}

} // namespace nexrx
