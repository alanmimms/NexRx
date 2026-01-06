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

bool AudioEngine::init(uint32_t sampleRate, uint32_t channels) {
    if (initialized_) {
        return true;
    }

    sampleRate_ = sampleRate;
    channels_ = channels;

    device_ = new ma_device();

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = channels;
    config.sampleRate = sampleRate;
    config.dataCallback = &AudioEngine::dataCallback;
    config.pUserData = this;

    ma_result result = ma_device_init(nullptr, &config, device_);
    if (result != MA_SUCCESS) {
        std::cerr << "Failed to initialize audio device: " << result << std::endl;
        delete device_;
        device_ = nullptr;
        return false;
    }

    initialized_ = true;
    std::cout << "Audio initialized: " << sampleRate << " Hz, " << channels << " channels" << std::endl;

    return true;
}

void AudioEngine::shutdown() {
    if (!initialized_) {
        return;
    }

    stop();

    if (device_) {
        ma_device_uninit(device_);
        delete device_;
        device_ = nullptr;
    }

    initialized_ = false;
}

bool AudioEngine::start() {
    if (!initialized_ || playing_.load()) {
        return playing_.load();
    }

    ma_result result = ma_device_start(device_);
    if (result != MA_SUCCESS) {
        std::cerr << "Failed to start audio device: " << result << std::endl;
        return false;
    }

    playing_.store(true);
    return true;
}

void AudioEngine::stop() {
    if (!initialized_ || !playing_.load()) {
        return;
    }

    ma_device_stop(device_);
    playing_.store(false);
}

void AudioEngine::setVolume(float volume) {
    volume_.store(std::clamp(volume, 0.0f, 1.0f));
}

void AudioEngine::setTestTone(bool enabled, float frequency) {
    testToneEnabled_.store(enabled);
    testToneFrequency_.store(frequency);
    if (!enabled) {
        testTonePhase_ = 0.0f;
    }
}

void AudioEngine::dataCallback(ma_device* device, void* output, const void* /*input*/, uint32_t frameCount) {
    AudioEngine* engine = static_cast<AudioEngine*>(device->pUserData);
    engine->processAudio(static_cast<float*>(output), frameCount);
}

void AudioEngine::processAudio(float* output, uint32_t frameCount) {
    const uint32_t channels = channels_;
    const uint32_t totalSamples = frameCount * channels;

    // Clear buffer first
    for (uint32_t i = 0; i < totalSamples; ++i) {
        output[i] = 0.0f;
    }

    // If muted, output silence
    if (muted_.load()) {
        return;
    }

    // Generate test tone if enabled
    if (testToneEnabled_.load()) {
        const float frequency = testToneFrequency_.load();
        const float phaseIncrement = 2.0f * 3.14159265359f * frequency / sampleRate_;

        for (uint32_t frame = 0; frame < frameCount; ++frame) {
            float sample = std::sin(testTonePhase_) * 0.3f;  // 0.3 amplitude to avoid clipping
            testTonePhase_ += phaseIncrement;

            // Keep phase in reasonable range
            if (testTonePhase_ > 2.0f * 3.14159265359f) {
                testTonePhase_ -= 2.0f * 3.14159265359f;
            }

            // Write to all channels
            for (uint32_t ch = 0; ch < channels; ++ch) {
                output[frame * channels + ch] = sample;
            }
        }
    }

    // Call user callback if set
    if (callback_) {
        // Create temporary buffer for callback
        std::vector<float> callbackBuffer(totalSamples, 0.0f);
        callback_(callbackBuffer.data(), frameCount, channels);

        // Mix callback output with existing output
        for (uint32_t i = 0; i < totalSamples; ++i) {
            output[i] += callbackBuffer[i];
        }
    }

    // Apply volume
    const float volume = volume_.load();
    for (uint32_t i = 0; i < totalSamples; ++i) {
        output[i] *= volume;

        // Soft clipping
        if (output[i] > 1.0f) output[i] = 1.0f;
        if (output[i] < -1.0f) output[i] = -1.0f;
    }
}
