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

      size_t i1 = static_cast<size_t>(sampleIdxF);
      size_t i2 = (i1 + 1);
      if (i2 >= totalSamples) {
        i2 = samplesRepeat ? 0 : totalSamples - 1;
      }
      double f = sampleIdxF - i1;
      double rawMod = audioSamples[i1] + f * (audioSamples[i2] - audioSamples[i1]);
      
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

void AMGenerator::generateBatch(double startTime, double samplePeriod, size_t count, double* outIQ, double stimGain) const {
  double phase0 = 2.0 * M_PI * std::fmod(carrierHz * startTime, 1.0);
  double cosP = std::cos(phase0);
  double sinP = std::sin(phase0);

  double phaseInc = 2.0 * M_PI * std::fmod(carrierHz * samplePeriod, 1.0);
  double cosInc = std::cos(phaseInc);
  double sinInc = std::sin(phaseInc);

  double effectiveAmp = amplitudeV * stimGain;

  if (audioSource == AudioSource::Tones) {
    struct TonePhasor {
      double amp;
      double cosP;
      double sinP;
      double cosInc;
      double sinInc;
    };
    std::vector<TonePhasor> tPhasors;
    tPhasors.reserve(tones.size());
    for (const auto& t : tones) {
      double p0 = 2.0 * M_PI * std::fmod(t.freqHz * startTime, 1.0);
      double pInc = 2.0 * M_PI * std::fmod(t.freqHz * samplePeriod, 1.0);
      tPhasors.push_back({t.amplitude, std::cos(p0), std::sin(p0), std::cos(pInc), std::sin(pInc)});
    }

    for (size_t i = 0; i < count; ++i) {
      double mod = 0.0;
      for (auto& tp : tPhasors) {
        mod += tp.amp * tp.sinP;
        double nc = tp.cosP * tp.cosInc - tp.sinP * tp.sinInc;
        double ns = tp.sinP * tp.cosInc + tp.cosP * tp.sinInc;
        tp.cosP = nc; tp.sinP = ns;
      }
      double env = effectiveAmp * (1.0 + modIndex * mod);

      outIQ[i * 2] += env * cosP;
      outIQ[i * 2 + 1] += env * sinP;

      double nextCos = cosP * cosInc - sinP * sinInc;
      double nextSin = sinP * cosInc + cosP * sinInc;
      cosP = nextCos;
      sinP = nextSin;

      if ((i & 1023) == 0) {
        double norm = 1.0 / std::sqrt(cosP * cosP + sinP * sinP);
        cosP *= norm;
        sinP *= norm;
        for (auto& tp : tPhasors) {
          double tnorm = 1.0 / std::sqrt(tp.cosP * tp.cosP + tp.sinP * tp.sinP);
          tp.cosP *= tnorm; tp.sinP *= tnorm;
        }
      }
    }
    return;
  }

  for (size_t i = 0; i < count; ++i) {
    double t = startTime + i * samplePeriod;
    double mod = getModulation(t);
    double env = effectiveAmp * (1.0 + modIndex * mod);

    outIQ[i * 2] += env * cosP;
    outIQ[i * 2 + 1] += env * sinP;

    double nextCos = cosP * cosInc - sinP * sinInc;
    double nextSin = sinP * cosInc + cosP * sinInc;
    cosP = nextCos;
    sinP = nextSin;

    if ((i & 1023) == 0) {
      double norm = 1.0 / std::sqrt(cosP * cosP + sinP * sinP);
      cosP *= norm;
      sinP *= norm;
    }
  }
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
