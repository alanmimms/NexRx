// NexRx Digital Twin - Stimulus Manager Implementation
//
// Copyright 2026 NexRx Project - MIT License

#include "StimulusManager.hpp"
#include "../ThreadPool.hpp"
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

      stim->generateBatch(startTime, samplePeriod, count, outIQ, stimGain);
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

void StimulusManager::generateBatchParallel(ThreadPool& pool, double startTime, double samplePeriod,
                                           size_t count, double* outIQ,
                                           double centerHz, double bandwidthHz,
                                           std::function<double(double)> gainFunc) const {
  std::fill(outIQ, outIQ + 2 * count, 0.0);

  struct ActiveItem {
    StimulusPtr stim;
    double gain;
  };
  std::vector<ActiveItem> activeItems;

  double g = globalGain.load();
  auto collectStim = [&](const StimulusPtr& stim) {
    if (!stim->isBroadband() && bandwidthHz > 0) {
      double stimFreq = stim->carrierFrequency();
      if (std::abs(stimFreq - centerHz) > bandwidthHz / 2.0) {
        return;
      }
    }

    double stimGain = g;
    if (gainFunc) {
      stimGain *= gainFunc(stim->carrierFrequency());
    }

    activeItems.push_back({stim, stimGain});
  };

  if (frozen.load()) {
    for (const auto& stim : frozenStimuli) {
      collectStim(stim);
    }
  } else {
    std::lock_guard<std::mutex> lock(stimulusMutex);
    for (const auto& pair : stimuli) {
      if (pair.second.enabled) {
        collectStim(pair.second.stimulus);
      }
    }
  }

  if (activeItems.empty()) {
    return;
  }

  if (activeItems.size() == 1 || pool.threadCount() <= 1) {
    for (const auto& item : activeItems) {
      item.stim->generateBatch(startTime, samplePeriod, count, outIQ, item.gain);
    }
    return;
  }

  int nItems = static_cast<int>(activeItems.size());
  int nExtra = nItems - 1;
  if (static_cast<int>(scratchBuffers.size()) < nExtra) {
    scratchBuffers.resize(nExtra);
  }
  for (int k = 0; k < nExtra; ++k) {
    if (scratchBuffers[k].size() != 2 * count) {
      scratchBuffers[k].resize(2 * count);
    }
    std::fill(scratchBuffers[k].begin(), scratchBuffers[k].end(), 0.0);
  }

  pool.parallelFor(nItems, [&](int idx) {
    if (idx == 0) {
      activeItems[0].stim->generateBatch(startTime, samplePeriod, count, outIQ, activeItems[0].gain);
    } else {
      activeItems[idx].stim->generateBatch(startTime, samplePeriod, count, scratchBuffers[idx - 1].data(), activeItems[idx].gain);
    }
  });

  int nTotal = static_cast<int>(2 * count);
  for (int k = 0; k < static_cast<int>(nExtra); ++k) {
    const double* src = scratchBuffers[k].data();
    for (int i = 0; i < nTotal; ++i) {
      outIQ[i] += src[i];
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
