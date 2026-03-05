// NexRx Digital Twin - AM Signal Generator Implementation
//
// Copyright 2026 NexRx Project - MIT License

#include "AMGenerator.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>

namespace nexrx {

AMGenerator::AMGenerator(double carrierHz, double amplitudeV)
  : carrierHz(carrierHz)
  , amplitudeV(amplitudeV) {
}

AMGenerator::~AMGenerator() = default;

void AMGenerator::setTones(const std::vector<double>& audioFreqsHz) {
  audioSource = AudioSource::Tones;
  tones.clear();
  
  if (audioFreqsHz.empty()) {
    return;
  }

  // Distribute amplitude equally among tones
  double ampPerTone = 1.0 / audioFreqsHz.size();
  for (double f : audioFreqsHz) {
    tones.push_back({f, ampPerTone});
  }
}

void AMGenerator::setAudioSamples(std::vector<float> samples, double sampleRate, bool repeat) {
  audioSource = AudioSource::Samples;
  audioSamples = std::move(samples);
  audioSampleRate = sampleRate;
  samplesRepeat = repeat;
}

double AMGenerator::getModulation(double timeS) const {
  switch (audioSource) {
    case AudioSource::Tones: {
      double sum = 0.0;
      for (const auto& tone : tones) {
        sum += tone.amplitude * std::cos(2.0 * M_PI * tone.freqHz * timeS);
      }
      return sum;
    }

    case AudioSource::Samples: {
      if (audioSamples.empty()) {
        return 0.0;
      }
      
      double sampleIdxF = timeS * audioSampleRate;
      size_t totalSamples = audioSamples.size();

      if (samplesRepeat) {
        sampleIdxF = std::fmod(sampleIdxF, static_cast<double>(totalSamples));
        if (sampleIdxF < 0) {
          sampleIdxF += totalSamples;
        }
      } else if (sampleIdxF >= totalSamples || sampleIdxF < 0) {
        return 0.0;
      }

      size_t idx0 = static_cast<size_t>(sampleIdxF);
      size_t idx1 = (idx0 + 1) % totalSamples;
      double frac = sampleIdxF - idx0;

      return audioSamples[idx0] * (1.0 - frac) + audioSamples[idx1] * frac;
    }

    default:
      return 0.0;
  }
}

double AMGenerator::getSample(double timeS) const {
  double mod = getModulation(timeS);
  double carrier = std::cos(2.0 * M_PI * carrierHz * timeS);
  return amplitudeV * (1.0 + modIndex * mod) * carrier;
}

void AMGenerator::getRfIQ(double timeS, double& outI, double& outQ) const {
  // Detect backward time jump or first call
  if (timeS < lastTime || lastTime < 0) {
    carrierPhase = std::fmod(2.0 * M_PI * carrierHz * timeS, 2.0 * M_PI);
  } else {
    double dt = timeS - lastTime;
    carrierPhase = std::fmod(carrierPhase + 2.0 * M_PI * carrierHz * dt, 2.0 * M_PI);
  }
  lastTime = timeS;

  double mod = getModulation(timeS);
  double env = amplitudeV * (1.0 + modIndex * mod);

  outI = env * std::cos(carrierPhase);
  outQ = env * std::sin(carrierPhase);
}

std::string AMGenerator::description() const {
  std::ostringstream oss;
  oss << "AM[" << carrierHz / 1e6 << "MHz, mod=" << (int)(modIndex * 100) << "%, ";
  switch (audioSource) {
    case AudioSource::Tones:
      oss << tones.size() << " tone(s)";
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

void AMGenerator::reset() {
  lastTime = -1.0;
  carrierPhase = 0.0;
}

} // namespace nexrx
