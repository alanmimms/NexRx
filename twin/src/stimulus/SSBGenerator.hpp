// NexRx Digital Twin - SSB Signal Generator
//
// Generates Single Sideband (USB/LSB) signals with multi-tone
// or voice modulation using Hilbert transform.
//
// Copyright 2026 NexRx Project - MIT License

#pragma once

#include "AntennaStimulus.hpp"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace nexrx {

// Forward declaration for TTS integration
class TTSEngine;

//======================================================================
// SSB Generator
//
// Generates USB or LSB signals from audio input (tones or voice).
// Uses phasing method: USB = I*cos - Q*sin, LSB = I*cos + Q*sin
// where Q is the Hilbert transform of I.
//======================================================================
class SSBGenerator : public AntennaStimulus {
public:
  enum class Mode { USB, LSB };

  // Constructor
  // carrierHz: suppressed carrier frequency
  // amplitudeV: peak RF amplitude
  // mode: USB or LSB
  SSBGenerator(double carrierHz, double amplitudeV, Mode mode);

  ~SSBGenerator() override;

  //------------------------------------------------------------------
  // Configuration
  //------------------------------------------------------------------

  void setCarrier(double freqHz) { carrierHz = freqHz; }
  void setAmplitude(double ampV) { amplitudeV = ampV; }
  void setMode(Mode m) { mode = m; }

  // Set audio as multiple tones (frequencies in Hz, typically 300-3000 Hz)
  void setTones(const std::vector<double>& audioFreqsHz);

  // Set audio from espeak-ng TTS engine
  void setVoice(std::shared_ptr<TTSEngine> tts, bool repeat = true);

  // Set audio from raw samples (sample rate must be specified)
  void setAudioSamples(std::vector<float> samples, double sampleRate, bool repeat = true);

  //------------------------------------------------------------------
  // AntennaStimulus interface
  //------------------------------------------------------------------

  [[nodiscard]] double getSample(double timeS) const override;
  [[nodiscard]] std::string description() const override;
  void reset() override;

  // Analytic RF signal (complex envelope at carrier)
  void getRfIQ(double timeS, double& outI, double& outQ) const override;
  [[nodiscard]] double carrierFrequency() const override { return carrierHz; }

  //------------------------------------------------------------------
  // Accessors
  //------------------------------------------------------------------

  [[nodiscard]] double getCarrier() const { return carrierHz; }
  [[nodiscard]] double getAmplitude() const { return amplitudeV; }
  [[nodiscard]] Mode getMode() const { return mode; }

private:
  // Audio source types
  enum class AudioSource { None, Tones, Voice, Samples };

  // Get audio I/Q at given time (Q is Hilbert of I)
  void getAudioIQ(double timeS, double& i, double& q) const;

  // Hilbert filter for arbitrary audio
  double hilbertFilter(double timeS) const;

  double carrierHz;
  double amplitudeV;
  Mode mode;

  AudioSource audioSource = AudioSource::None;

  // Tone mode
  struct ToneInfo {
    double freqHz;
    double amplitude;  // Normalized (sum = 1)
  };
  std::vector<ToneInfo> tones;

  // Voice mode (espeak-ng)
  std::shared_ptr<TTSEngine> tts;
  bool voiceRepeat = true;

  // Sample mode
  std::vector<float> audioSamples;
  std::vector<float> audioSamplesQ;  // Pre-computed Hilbert transform
  double audioSampleRate = 0.0;
  bool samplesRepeat = true;

  // Hilbert filter coefficients (FIR approximation)
  static constexpr size_t HILBERT_TAPS = 65;
  std::vector<double> hilbertCoeffs;
  mutable std::vector<double> hilbertHistory;
  mutable size_t hilbertIndex = 0;
  mutable double lastSampleTime = -1.0;

  mutable double carrierPhase = 0.0;
  mutable double lastTime = -1.0;
  mutable double modFiltState1 = 0.0;
  mutable double modFiltState2 = 0.0;
  mutable double qFiltState1 = 0.0;
  mutable double qFiltState2 = 0.0;

  void initHilbertFilter();
  void precomputeHilbert();  // Pre-compute Q channel for samples
  void resampleToInternalRate(const std::vector<float>& input, double inputRate);
  void applyLoopCrossfade();  // Smooth loop boundary to prevent transients
};

} // namespace nexrx
