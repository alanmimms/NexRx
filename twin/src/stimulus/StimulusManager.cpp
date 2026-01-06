// NexRx Digital Twin - Stimulus Manager Implementation
//
// Copyright 2026 NexRx Project - MIT License

#include "StimulusManager.hpp"

#include <algorithm>

namespace nexrx {

void StimulusManager::addStimulus(const std::string& name, StimulusPtr stimulus,
                                   const std::string& type,
                                   double freq_hz, double amplitude_v) {
    std::lock_guard<std::mutex> lock(mutex_);

    Entry entry;
    entry.stimulus = std::move(stimulus);
    entry.info.name = name;
    entry.info.type = type;
    entry.info.frequency_hz = freq_hz;
    entry.info.amplitude_v = amplitude_v;
    entry.info.active = true;
    entry.enabled = true;

    stimuli_[name] = std::move(entry);
}

bool StimulusManager::removeStimulus(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    return stimuli_.erase(name) > 0;
}

void StimulusManager::clearAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    stimuli_.clear();
}

bool StimulusManager::hasStimulus(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stimuli_.find(name) != stimuli_.end();
}

std::vector<std::string> StimulusManager::listNames() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    names.reserve(stimuli_.size());
    for (const auto& [name, entry] : stimuli_) {
        names.push_back(name);
    }
    return names;
}

std::vector<StimulusInfo> StimulusManager::listInfo() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<StimulusInfo> infos;
    infos.reserve(stimuli_.size());
    for (const auto& [name, entry] : stimuli_) {
        infos.push_back(entry.info);
    }
    return infos;
}

size_t StimulusManager::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stimuli_.size();
}

double StimulusManager::getSample(double time_s) const {
    std::lock_guard<std::mutex> lock(mutex_);

    double sum = 0.0;
    for (const auto& [name, entry] : stimuli_) {
        if (entry.enabled && entry.stimulus) {
            sum += entry.stimulus->getSample(time_s);
        }
    }
    return sum;
}

void StimulusManager::setEnabled(const std::string& name, bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = stimuli_.find(name);
    if (it != stimuli_.end()) {
        it->second.enabled = enabled;
        it->second.info.active = enabled;
    }
}

void StimulusManager::setAllEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [name, entry] : stimuli_) {
        entry.enabled = enabled;
        entry.info.active = enabled;
    }
}

void StimulusManager::resetAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [name, entry] : stimuli_) {
        if (entry.stimulus) {
            entry.stimulus->reset();
        }
    }
}

} // namespace nexrx
