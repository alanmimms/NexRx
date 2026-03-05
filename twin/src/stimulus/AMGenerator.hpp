// NexRx Digital Twin - AM Signal Generator
//
// Generates Amplitude Modulated (AM) signals with multi-tone
// or voice modulation.
//
// Copyright 2026 NexRx Project - MIT License

#pragma once

#include "AntennaStimulus.hpp"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace nexrx {

//======================================================================
// AM Generator
//
// Generates AM signals: RF = Ac * (1 + m * modulation) * cos(wc * t)
//======================================================================
class AMGenerator : public AntennaStimulus {
public:
  // Constructor
  // carrierHz: carrier frequency
  // amplitudeV: peak RF amplitude (unmodulated carrier)
  AMGenerator(double carrierHz, double amplitudeV);

  ~AMGenerator() override;

  //------------------------------------------------------------------
  // Configuration
  //------------------------------------------------------------------

  void setCarrier(double freqHz) { carrierHz = freqHz; }
  void setAmplitude(double ampV) { amplitudeV = ampV; }
  void setModulationIndex(double m) { modIndex = m; }

  // Set modulation as multiple tones (frequencies in Hz)
  void setTones(const std::vector<double>& audioFreqsHz);

  // Set modulation from raw samples (sample rate must be specified)
  void setAudioSamples(std::vector<float> samples, double sampleRate, bool repeat = true);

  //------------------------------------------------------------------
  // AntennaStimulus interface
  //------------------------------------------------------------------

  [[nodiscard]] double getSample(double timeS) const override;
  [[nodiscard]] std::string description() const override;
  void reset() override;

  // Analytic RF signal
  void getRfIQ(double timeS, double& outI, double& outQ) const override;
  [[nodiscard]] double carrierFrequency() const override { return carrierHz; }

private:
  // Audio source types
  enum class AudioSource { None, Tones, Samples };

  // Get modulation signal at given time (range -1 to 1)
  double getModulation(double timeS) const;

  double carrierHz;
  double amplitudeV;
  double modIndex = 0.8; // Default 80% modulation

  AudioSource audioSource = AudioSource::None;

  // Tone mode
  struct ToneInfo {
    double freqHz;
    double amplitude;
  };
  std::vector<ToneInfo> tones;

  // Sample mode
  std::vector<float> audioSamples;
  double audioSampleRate = 0.0;
  bool samplesRepeat = true;

  mutable double carrierPhase = 0.0;
  mutable double lastTime = -1.0;
};

} // namespace nexrx
