/**
 * @file AudioEngine.hpp
 * @brief Audio playback engine using miniaudio
 */

#pragma once

#include <string>
#include <functional>
#include <atomic>
#include <cstdint>
#include <vector>

// Forward declare miniaudio types to avoid including the large header here
struct ma_device;
struct ma_device_config;

/**
 * @brief Audio callback type for generating/processing audio samples
 * @param output Pointer to output buffer (interleaved float samples)
 * @param frameCount Number of frames to generate
 * @param channels Number of channels (typically 2 for stereo)
 */
using AudioCallback = std::function<void(float* output, uint32_t frameCount, uint32_t channels)>;

/**
 * @brief Simple audio playback engine
 */
class AudioEngine {
public:
  AudioEngine();
  ~AudioEngine();

  // Non-copyable
  AudioEngine(const AudioEngine&) = delete;
  AudioEngine& operator=(const AudioEngine&) = delete;

  /**
   * @brief Initialize the audio device
   * @param sampleRate Desired sample rate (e.g., 48000)
   * @param channels Number of channels (1 for mono, 2 for stereo)
   * @return true if successful
   */
  bool init(uint32_t sampleRate = 48000, uint32_t channels = 2);

  /**
   * @brief Shutdown the audio device
   */
  void shutdown();

  /**
   * @brief Start audio playback
   * @return true if successful
   */
  bool start();

  /**
   * @brief Stop audio playback
   */
  void stop();

  /**
   * @brief Check if audio is currently playing
   */
  bool isPlaying() const { return playing.load(); }

  /**
   * @brief Set the audio callback for generating samples
   */
  void setCallback(AudioCallback callback) { this->callback = std::move(callback); }

  /**
   * @brief Set master volume (0.0 to 1.0)
   */
  void setVolume(float volume);

  /**
   * @brief Get current volume
   */
  float getVolume() const { return volume.load(); }

  /**
   * @brief Set mute state
   */
  void setMuted(bool muted) { this->muted.store(muted); }

  /**
   * @brief Get mute state
   */
  bool isMuted() const { return muted.load(); }

  /**
   * @brief Get current sample rate
   */
  uint32_t getSampleRate() const { return sampleRate; }

  /**
   * @brief Get number of channels
   */
  uint32_t getChannels() const { return channels; }

  /**
   * @brief Check if engine is initialized
   */
  bool isInitialized() const { return initialized; }

  // --- Built-in test tone generator ---

  /**
   * @brief Enable/disable test tone
   * @param enabled Whether to play test tone
   * @param frequency Tone frequency in Hz (default 440 Hz = A4)
   */
  void setTestTone(bool enabled, float frequency = 440.0f);

  /**
   * @brief Check if test tone is enabled
   */
  bool isTestToneEnabled() const { return testToneEnabled.load(); }

private:
  // Called by miniaudio
  static void dataCallback(ma_device* device, void* output, const void* input, uint32_t frameCount);
  void processAudio(float* output, uint32_t frameCount);

  ma_device* device = nullptr;
  AudioCallback callback;

  uint32_t sampleRate = 48000;
  uint32_t channels = 2;
  bool initialized = false;

  std::atomic<bool> playing{false};
  std::atomic<float> volume{0.8f};
  std::atomic<bool> muted{false};

  // Test tone state
  std::atomic<bool> testToneEnabled{false};
  std::atomic<float> testToneFrequency{440.0f};
  float testTonePhase = 0.0f;

  // Pre-allocated buffer for callback (avoid allocation in audio thread)
  std::vector<float> callbackBuffer;
};
