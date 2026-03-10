/**
 * @file AudioEngine.cpp
 * @brief Audio playback engine implementation
 */

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio/miniaudio.h"

#include "AudioEngine.hpp"

#include <iostream>
#include <cmath>
#include <algorithm>

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine() {
  shutdown();
}

bool AudioEngine::init(uint32_t sampleRateIn, uint32_t channelsIn) {
  if (initialized) {
    return true;
  }

  sampleRate = sampleRateIn;
  channels = channelsIn;

  device = new ma_device();

  ma_device_config config = ma_device_config_init(ma_device_type_playback);
  config.playback.format = ma_format_f32;
  config.playback.channels = channels;
  config.sampleRate = sampleRate;
  config.dataCallback = &AudioEngine::dataCallback;
  config.pUserData = this;

  // Increase period size for better VM/Windows compatibility
  config.periodSizeInMilliseconds = 40; 
  config.periods = 3;

  ma_result result = ma_device_init(nullptr, &config, device);
  if (result != MA_SUCCESS) {
    std::cerr << "[AudioEngine] Failed to initialize audio device: " << result << std::endl;
    delete device;
    device = nullptr;
    return false;
  }

  initialized = true;

  // Pre-allocate callback buffer
  callbackBuffer.resize(8192 * channels, 0.0f);

  return true;
}

void AudioEngine::shutdown() {
  if (!initialized) {
    return;
  }

  stop();

  if (device) {
    ma_device_uninit(device);
    delete device;
    device = nullptr;
  }

  initialized = false;
}

bool AudioEngine::start() {
  if (!initialized) {
    std::cerr << "[AudioEngine] Cannot start: not initialized" << std::endl;
    return false;
  }
  if (playing.load()) return true;

  ma_result result = ma_device_start(device);
  if (result != MA_SUCCESS) {
    std::cerr << "[AudioEngine] Failed to start audio device: " << result << std::endl;
    return false;
  }

  playing.store(true);
  return true;
}

void AudioEngine::stop() {
  if (!initialized || !playing.load()) {
    return;
  }

  ma_device_stop(device);
  playing.store(false);
}

void AudioEngine::setVolume(float vol) {
  volume.store(std::clamp(vol, 0.0f, 2.0f));
}

void AudioEngine::setTestTone(bool enabled, float frequency) {
  testToneEnabled.store(enabled);
  testToneFrequency.store(frequency);
  if (!enabled) {
    testTonePhase = 0.0f;
  }
}

void AudioEngine::dataCallback(ma_device* dev, void* output, const void* /*input*/, uint32_t frameCount) {
  AudioEngine* engine = static_cast<AudioEngine*>(dev->pUserData);
  engine->processAudio(static_cast<float*>(output), frameCount);
}

void AudioEngine::processAudio(float* output, uint32_t frameCount) {
  const uint32_t chs = channels;
  const uint32_t totalSamples = frameCount * chs;

  // 1. Get DSP audio if available
  std::fill(callbackBuffer.begin(), callbackBuffer.begin() + totalSamples, 0.0f);
  if (callback) {
    callback(callbackBuffer.data(), frameCount, chs);
  }

  // 2. Mix and apply Volume/Mute/Tone
  float vol = muted.load() ? 0.0f : volume.load();
  bool tone = testToneEnabled.load();
  float freq = testToneFrequency.load();
  float phaseInc = 2.0f * M_PI * freq / sampleRate;

  for (uint32_t frame = 0; frame < frameCount; ++frame) {
    float toneSample = 0.0f;
    if (testToneEnabled.load()) {
      toneSample = 0.2f * std::sin(testTonePhase);
      testTonePhase += phaseInc;
      if (testTonePhase > 2.0f * M_PI) testTonePhase -= 2.0f * M_PI;
    }

    float v = muted.load() ? 0.0f : volume.load();
    for (uint32_t ch = 0; ch < chs; ++ch) {
      float sample = callbackBuffer[frame * chs + ch] * v + toneSample;
      
      // Final safety clamp
      if (sample > 1.0f) sample = 1.0f;
      if (sample < -1.0f) sample = -1.0f;
      output[frame * chs + ch] = sample;
    }
  }
}
