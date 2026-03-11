// NexRx Digital Twin - Stimulus Manager Implementation
//
// Copyright 2026 NexRx Project - MIT License

#include "StimulusManager.hpp"
#include <algorithm>
#include <iostream>

namespace nexrx {

void StimulusManager::addStimulus(const std::string& name, StimulusPtr stimulus,
                                const std::string& type, double freqHz, double amplitudeV) {
  std::lock_guard<std::mutex> lock(stimulusMutex);
  
  StimulusInfo info;
  info.name = name;
  info.type = type;
  info.frequencyHz = freqHz;
  info.amplitudeV = amplitudeV;
  info.active = true;

  stimuli[name] = {std::move(stimulus), info, true};
  
  if (frozen.load()) {
    unfreeze();
  }
}

bool StimulusManager::removeStimulus(const std::string& name) {
  std::lock_guard<std::mutex> lock(stimulusMutex);
  auto it = stimuli.find(name);
  if (it != stimuli.end()) {
    stimuli.erase(it);
    if (frozen.load()) {
      unfreeze();
    }
    return true;
  }
  return false;
}

void StimulusManager::clearAll() {
  std::lock_guard<std::mutex> lock(stimulusMutex);
  stimuli.clear();
  if (frozen.load()) {
    unfreeze();
  }
}

bool StimulusManager::hasStimulus(const std::string& name) const {
  std::lock_guard<std::mutex> lock(stimulusMutex);
  return stimuli.find(name) != stimuli.end();
}

std::vector<std::string> StimulusManager::listNames() const {
  std::lock_guard<std::mutex> lock(stimulusMutex);
  std::vector<std::string> names;
  for (const auto& pair : stimuli) {
    names.push_back(pair.first);
  }
  return names;
}

std::vector<StimulusInfo> StimulusManager::listInfo() const {
  std::lock_guard<std::mutex> lock(stimulusMutex);
  std::vector<StimulusInfo> infoList;
  for (const auto& pair : stimuli) {
    infoList.push_back(pair.second.info);
  }
  return infoList;
}

size_t StimulusManager::count() const {
  std::lock_guard<std::mutex> lock(stimulusMutex);
  return stimuli.size();
}

double StimulusManager::getSample(double timeS) const {
  if (frozen.load()) {
    double sum = 0.0;
    for (const auto& stim : frozenStimuli) {
      sum += stim->getSample(timeS);
    }
    return sum * globalGain.load();
  }

  std::lock_guard<std::mutex> lock(stimulusMutex);
  double sum = 0.0;
  for (const auto& pair : stimuli) {
    if (pair.second.enabled) {
      sum += pair.second.stimulus->getSample(timeS);
    }
  }
  return sum * globalGain.load();
}

void StimulusManager::getRfIQ(double timeS, double& outI, double& outQ, 
                             double centerHz, double bandwidthHz) const {
  outI = 0.0;
  outQ = 0.0;

  if (frozen.load()) {
    for (const auto& stim : frozenStimuli) {
      if (stim->isBroadband() || bandwidthHz == 0 || std::abs(stim->carrierFrequency() - centerHz) <= bandwidthHz / 2.0) {
        double i, q;
        stim->getRfIQ(timeS, i, q);
        outI += i;
        outQ += q;
      }
    }
    double g = globalGain.load();
    outI *= g;
    outQ *= g;
    return;
  }

  std::lock_guard<std::mutex> lock(stimulusMutex);
  for (const auto& pair : stimuli) {
    if (pair.second.enabled) {
      auto stim = pair.second.stimulus;
      if (stim->isBroadband() || bandwidthHz == 0 || std::abs(stim->carrierFrequency() - centerHz) <= bandwidthHz / 2.0) {
        double i, q;
        stim->getRfIQ(timeS, i, q);
        outI += i;
        outQ += q;
      }
    }
  }
  double g = globalGain.load();
  outI *= g;
  outQ *= g;
}

void StimulusManager::freeze() {
  std::lock_guard<std::mutex> lock(stimulusMutex);
  frozenStimuli.clear();
  for (const auto& pair : stimuli) {
    if (pair.second.enabled) {
      frozenStimuli.push_back(pair.second.stimulus);
    }
  }
  frozen.store(true);
}

void StimulusManager::unfreeze() {
  frozen.store(false);
}

void StimulusManager::getRfIQFast(double timeS, double& outI, double& outQ,
                                 double centerHz, double bandwidthHz) const {
  outI = 0.0;
  outQ = 0.0;
  for (const auto& stim : frozenStimuli) {
    if (stim->isBroadband() || bandwidthHz == 0 || std::abs(stim->carrierFrequency() - centerHz) <= bandwidthHz / 2.0) {
      double i, q;
      stim->getRfIQ(timeS, i, q);
      outI += i;
      outQ += q;
    }
  }
  double g = globalGain.load();
  outI *= g;
  outQ *= g;
}

void StimulusManager::generateBatch(double startTime, double samplePeriod,
                                   size_t count, double* outIQ,
                                   double centerHz, double bandwidthHz,
                                   std::function<double(double)> gainFunc) const {
  std::fill(outIQ, outIQ + 2 * count, 0.0);
  
  double g = globalGain.load();
  auto applyStim = [&](const StimulusPtr& stim) {
      // Nyquist window check for digital twin simulation
      if (!stim->isBroadband() && bandwidthHz > 0) {
          double stimFreq = stim->carrierFrequency();
          if (std::abs(stimFreq - centerHz) > bandwidthHz / 2.0) {
              return; // Skip out-of-band stimulus to avoid aliasing
          }
      }

      // Frequency-dependent gain (e.g. Preselector)
      double stimGain = g;
      if (gainFunc) {
          stimGain *= gainFunc(stim->carrierFrequency());
      }

      for (size_t i = 0; i < count; ++i) {
        double t = startTime + i * samplePeriod;
        double sI, sQ;
        stim->getRfIQ(t, sI, sQ);
        outIQ[i * 2] += sI * stimGain;
        outIQ[i * 2 + 1] += sQ * stimGain;
      }
  };

  if (frozen.load()) {
    for (const auto& stim : frozenStimuli) {
      applyStim(stim);
    }
  } else {
    std::lock_guard<std::mutex> lock(stimulusMutex);
    for (const auto& pair : stimuli) {
      if (pair.second.enabled) {
        applyStim(pair.second.stimulus);
      }
    }
  }
}

void StimulusManager::setEnabled(const std::string& name, bool enabled) {
  std::lock_guard<std::mutex> lock(stimulusMutex);
  auto it = stimuli.find(name);
  if (it != stimuli.end()) {
    it->second.enabled = enabled;
    if (frozen.load()) {
      unfreeze();
    }
  }
}

void StimulusManager::setAllEnabled(bool enabled) {
  std::lock_guard<std::mutex> lock(stimulusMutex);
  for (auto& pair : stimuli) {
    pair.second.enabled = enabled;
  }
  if (frozen.load()) {
    unfreeze();
  }
}

void StimulusManager::resetAll() {
  std::lock_guard<std::mutex> lock(stimulusMutex);
  for (auto& pair : stimuli) {
    pair.second.stimulus->reset();
  }
}

bool StimulusManager::isAnyWithin(double centerHz, double bandwidthHz) const {
  if (frozen.load()) {
    for (const auto& stim : frozenStimuli) {
      if (stim->isBroadband() || std::abs(stim->carrierFrequency() - centerHz) <= bandwidthHz / 2.0) {
        return true;
      }
    }
    return false;
  }

  std::lock_guard<std::mutex> lock(stimulusMutex);
  for (const auto& pair : stimuli) {
    if (pair.second.enabled) {
      if (pair.second.stimulus->isBroadband() || std::abs(pair.second.stimulus->carrierFrequency() - centerHz) <= bandwidthHz / 2.0) {
        return true;
      }
    }
  }
  return false;
}

double StimulusManager::carrierFrequency() const {
  if (frozen.load()) {
    if (frozenStimuli.empty()) return 0.0;
    return frozenStimuli[0]->carrierFrequency();
  }

  std::lock_guard<std::mutex> lock(stimulusMutex);
  for (const auto& pair : stimuli) {
    if (pair.second.enabled) {
      return pair.second.stimulus->carrierFrequency();
    }
  }
  return 0.0;
}

} // namespace nexrx
