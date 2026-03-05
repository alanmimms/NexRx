// NexRx Digital Twin - Text-to-Speech Engine Implementation
//
// Copyright 2026 NexRx Project - MIT License

#include "TTSEngine.hpp"

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

bool TTSEngine::initialized = false;

bool TTSEngine::initEspeak() {
#if HAS_ESPEAK_NG
  if (initialized) {
    return true;
  }

  int result = espeak_Initialize(AUDIO_OUTPUT_SYNCHRONOUS, 0, nullptr, 0);
  if (result >= 0) {
    initialized = true;
    return true;
  }
  return false;
#else
  return false;
#endif
}

bool TTSEngine::isAvailable() {
#if HAS_ESPEAK_NG
  return initEspeak();
#else
  return false;
#endif
}

TTSEngine::TTSEngine() {
  initEspeak();
}

TTSEngine::~TTSEngine() {
  // Don't terminate espeak - it's shared
}

void TTSEngine::setText(const std::string& t) {
  if (t != text) {
    text = t;
    needsSynthesize = true;
  }
}

void TTSEngine::setRate(int wpm) {
  if (wpm != rate) {
    rate = std::clamp(wpm, 80, 450);
    needsSynthesize = true;
  }
}

void TTSEngine::setPitch(int p) {
  if (p != pitch) {
    pitch = std::clamp(p, 0, 100);
    needsSynthesize = true;
  }
}

void TTSEngine::setVoiceName(const std::string& name) {
  if (name != voiceName) {
    voiceName = name;
    needsSynthesize = true;
  }
}

void TTSEngine::setRange(int r) {
  if (r != range) {
    range = std::clamp(r, 0, 100);
    needsSynthesize = true;
  }
}

void TTSEngine::setVolume(int v) {
  if (v != volume) {
    volume = std::clamp(v, 0, 200);
    needsSynthesize = true;
  }
}

void TTSEngine::setWordGap(int gap) {
  if (gap != wordGap) {
    wordGap = std::max(0, gap);
    needsSynthesize = true;
  }
}

void TTSEngine::setCapitals(int mode) {
  if (mode != capitals) {
    capitals = std::clamp(mode, 0, 3);
    needsSynthesize = true;
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

void TTSEngine::synthesizeInternal() {
  if (!needsSynthesize) {
    return;
  }
  needsSynthesize = false;
  samples.clear();

  if (text.empty()) {
    return;
  }

#if HAS_ESPEAK_NG
  if (initialized) {
    // Set voice by name
    if (!voiceName.empty()) {
      espeak_SetVoiceByName(voiceName.c_str());
    }

    // Configure espeak parameters
    espeak_SetParameter(espeakRATE, rate, 0);
    espeak_SetParameter(espeakPITCH, pitch, 0);
    espeak_SetParameter(espeakRANGE, range, 0);
    espeak_SetParameter(espeakVOLUME, volume, 0);
    espeak_SetParameter(espeakWORDGAP, wordGap, 0);
    espeak_SetParameter(espeakCAPITALS, capitals, 0);

    // Set up callback
    g_sampleBuffer = &samples;
    espeak_SetSynthCallback(espeakCallback);

    // Synthesize
    unsigned int flags = espeakCHARS_AUTO | espeakPHONEMES | espeakENDPAUSE;
    espeak_Synth(text.c_str(), text.size() + 1, 0, POS_CHARACTER, 0, flags, nullptr, nullptr);
    espeak_Synchronize();

    g_sampleBuffer = nullptr;

    // Get actual sample rate from espeak
    sampleRate = espeak_ng_GetSampleRate();
    if (sampleRate <= 0) {
      sampleRate = 22050.0;
    }

    return;
  }
#endif

  // Fallback: generate a simple beep pattern as placeholder
  sampleRate = 22050.0;

  // Generate 1 second of 1kHz tone per 10 characters
  double duration = std::max(1.0, text.size() / 10.0);
  size_t numSamples = static_cast<size_t>(duration * sampleRate);
  samples.resize(numSamples);

  for (size_t i = 0; i < numSamples; ++i) {
    double t = i / sampleRate;
    // Simple envelope
    double env = 1.0;
    if (t < 0.05) {
      env = t / 0.05;
    }
    if (t > duration - 0.05) {
      env = (duration - t) / 0.05;
    }

    // 1kHz tone with slight frequency variation
    double freq = 1000.0 + 100.0 * std::sin(2.0 * M_PI * 0.5 * t);
    samples[i] = static_cast<float>(env * 0.5 * std::sin(2.0 * M_PI * freq * t));
  }
}

std::vector<float> TTSEngine::synthesize(const std::string& t) {
  setText(t);
  synthesizeInternal();
  return samples;
}

float TTSEngine::getSample(double timeS) const {
  // Ensure samples are ready (const_cast for lazy synthesis)
  const_cast<TTSEngine*>(this)->synthesizeInternal();

  if (samples.empty()) {
    return 0.0f;
  }

  double duration = samples.size() / sampleRate;

  if (!repeat && timeS >= duration) {
    return 0.0f;
  }

  // Calculate sample position with repeat
  double effectiveTime = repeat ? std::fmod(timeS, duration) : timeS;
  if (effectiveTime < 0) {
    effectiveTime += duration;
  }

  double samplePos = effectiveTime * sampleRate;
  size_t idx0 = static_cast<size_t>(samplePos);
  size_t idx1 = (idx0 + 1) % samples.size();
  double frac = samplePos - idx0;

  // Linear interpolation
  return samples[idx0] * (1.0f - static_cast<float>(frac))
       + samples[idx1] * static_cast<float>(frac);
}

double TTSEngine::getDuration() const {
  const_cast<TTSEngine*>(this)->synthesizeInternal();
  if (samples.empty() || sampleRate <= 0) {
    return 0.0;
  }
  return samples.size() / sampleRate;
}

void TTSEngine::reset() {
  // Nothing to reset - synthesis is time-indexed
}

} // namespace nexrx
