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
        if (sampleIdxF < 0) sampleIdxF += totalSamples;
      } else if (sampleIdxF >= totalSamples || sampleIdxF < 0) {
        return 0.0;
      }

      // 4-tap cubic Hermite interpolation
      size_t i1 = static_cast<size_t>(sampleIdxF);
      size_t i0 = (i1 > 0) ? i1 - 1 : (samplesRepeat ? totalSamples - 1 : 0);
      size_t i2 = (i1 + 1) % totalSamples;
      size_t i3 = (i2 + 1) % totalSamples;
      double f = sampleIdxF - i1;

      double y0 = audioSamples[i0], y1 = audioSamples[i1], y2 = audioSamples[i2], y3 = audioSamples[i3];
      double a = -0.5*y0 + 1.5*y1 - 1.5*y2 + 0.5*y3;
      double b = y0 - 2.5*y1 + 2.0*y2 - 0.5*y3;
      double c = -0.5*y0 + 0.5*y2;
      double d = y1;
      double rawMod = ((a * f + b) * f + c) * f + d;
      
      // 2nd-order IIR LPF (two cascaded poles)
      // Cutoff ~4kHz at 480kHz -> alpha ~= 0.05
      constexpr double alpha = 0.05;
      modFiltState1 = (1.0 - alpha) * modFiltState1 + alpha * rawMod;
      modFiltState2 = (1.0 - alpha) * modFiltState2 + alpha * modFiltState1;
      return modFiltState2;
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
  double phase = 2.0 * M_PI * std::fmod(carrierHz * timeS, 1.0);
  double mod = getModulation(timeS);
  double env = amplitudeV * (1.0 + modIndex * mod);

  outI = env * std::cos(phase);
  outQ = env * std::sin(phase);
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
  modFiltState1 = 0.0;
  modFiltState2 = 0.0;
}

} // namespace nexrx
