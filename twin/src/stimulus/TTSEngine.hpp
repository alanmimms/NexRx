// NexRx Digital Twin - Text-to-Speech Engine
//
// Generates audio samples from text using espeak-ng.
// Provides continuous sample access for SSB modulation.
//
// Copyright 2026 NexRx Project - MIT License

#pragma once

#include <string>
#include <vector>

namespace nexrx {

//======================================================================
// TTS Engine
//
// Text-to-speech synthesis for SSB voice modulation.
// Uses espeak-ng library for cross-platform TTS.
//======================================================================
class TTSEngine {
public:
  TTSEngine();
  ~TTSEngine();

  // Non-copyable (owns espeak resources)
  TTSEngine(const TTSEngine&) = delete;
  TTSEngine& operator=(const TTSEngine&) = delete;

  //------------------------------------------------------------------
  // Configuration
  //------------------------------------------------------------------

  // Set the text to synthesize
  void setText(const std::string& text);

  // Set voice by name (e.g., "en", "en-us", "de", "es")
  void setVoiceName(const std::string& name);

  // Set speech rate in words per minute (default 175, range 80-450)
  void setRate(int wpm);

  // Set pitch (0-100, default 50)
  void setPitch(int pitch);

  // Set pitch range (0-100, default 50)
  void setRange(int range);

  // Set volume (0-200, default 100)
  void setVolume(int volume);

  // Set word gap in 10ms units (default 0)
  void setWordGap(int gap);

  // Set capitals mode (0=none, 1=sound icon, 2=pitch, 3=both)
  void setCapitals(int mode);

  // Set whether to repeat (default true)
  void setRepeat(bool r) { repeat = r; }

  //------------------------------------------------------------------
  // Audio access
  //------------------------------------------------------------------

  // Get audio sample at given time (sample rate is getSampleRate())
  // Returns 0 if no text set or past end in non-repeat mode
  [[nodiscard]] float getSample(double timeS) const;

  // Get the sample rate of generated audio
  [[nodiscard]] double getSampleRate() const { return sampleRate; }

  // Get total duration of the synthesized audio
  [[nodiscard]] double getDuration() const;

  // Reset playback to beginning
  void reset();

  //------------------------------------------------------------------
  // Direct synthesis (for pre-rendering)
  //------------------------------------------------------------------

  // Synthesize text and return all samples
  [[nodiscard]] std::vector<float> synthesize(const std::string& text);

  // Check if TTS is available (espeak-ng initialized)
  [[nodiscard]] static bool isAvailable();

private:
  void synthesizeInternal();

  std::string text;
  std::string voiceName = "en";  // espeak voice name
  int rate = 175;      // Words per minute (80-450)
  int pitch = 50;      // 0-100
  int range = 50;      // Pitch range 0-100
  int volume = 100;    // 0-200
  int wordGap = 0;     // Gap between words in 10ms units
  int capitals = 0;    // 0=none, 1=icon, 2=pitch, 3=both
  bool repeat = true;

  std::vector<float> samples;
  double sampleRate = 22050.0;
  bool needsSynthesize = true;

  static bool initialized;
  static bool initEspeak();
};

} // namespace nexrx
