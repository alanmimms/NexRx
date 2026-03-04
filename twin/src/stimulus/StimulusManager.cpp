// NexRx Digital Twin - Stimulus Manager Implementation
//
// Copyright 2026 NexRx Project - MIT License

#include "StimulusManager.hpp"

#include <algorithm>
#include <cmath>

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
    return sum * global_gain_.load();
}

void StimulusManager::getRfIQ(double time_s, double& out_i, double& out_q,
                               double center_hz, double bandwidth_hz) const {
    // Use fast path if frozen
    if (frozen_.load(std::memory_order_relaxed)) {
        getRfIQ_fast(time_s, out_i, out_q, center_hz, bandwidth_hz);
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    out_i = out_q = 0.0;
    for (const auto& [name, entry] : stimuli_) {
        if (entry.enabled && entry.stimulus) {
            // Roofing Filter: check if stimulus is within passband
            if (bandwidth_hz > 0) {
                double f = entry.stimulus->carrierFrequency();
                if (f != 0 && std::abs(f - center_hz) > bandwidth_hz / 2.0) {
                    continue; // Out of band
                }
            }

            double i, q;
            entry.stimulus->getRfIQ(time_s, i, q);
            out_i += i;
            out_q += q;
        }
    }
    double g = global_gain_.load();
    out_i *= g;
    out_q *= g;
}

void StimulusManager::freeze() {
    std::lock_guard<std::mutex> lock(mutex_);

    // Create snapshot of enabled stimuli
    frozenStimuli_.clear();
    frozenStimuli_.reserve(stimuli_.size());
    for (const auto& [name, entry] : stimuli_) {
        if (entry.enabled && entry.stimulus) {
            frozenStimuli_.push_back(entry.stimulus);
        }
    }

    frozen_.store(true, std::memory_order_release);
}

void StimulusManager::unfreeze() {
    frozen_.store(false, std::memory_order_release);
    frozenStimuli_.clear();
}

void StimulusManager::getRfIQ_fast(double time_s, double& out_i, double& out_q,
                                   double center_hz, double bandwidth_hz) const {
    // Lock-free access to frozen snapshot
    out_i = out_q = 0.0;
    for (const auto& stim : frozenStimuli_) {
        // Roofing Filter: check if stimulus is within passband
        if (bandwidth_hz > 0) {
            double f = stim->carrierFrequency();
            if (f != 0 && std::abs(f - center_hz) > bandwidth_hz / 2.0) {
                continue; // Out of band
            }
        }

        double i, q;
        stim->getRfIQ(time_s, i, q);
        out_i += i;
        out_q += q;
    }
    double g = global_gain_.load();
    out_i *= g;
    out_q *= g;
}

void StimulusManager::generateBatch(double start_time, double sample_period,
                                     size_t count, double* out_iq) const {
    // Initialize output to zero
    for (size_t i = 0; i < count * 2; ++i) {
        out_iq[i] = 0.0;
    }

    // Sum contributions from all frozen stimuli
    for (const auto& stim : frozenStimuli_) {
        double t = start_time;
        for (size_t i = 0; i < count; ++i) {
            double rf_i, rf_q;
            stim->getRfIQ(t, rf_i, rf_q);
            out_iq[i * 2] += rf_i;
            out_iq[i * 2 + 1] += rf_q;
            t += sample_period;
        }
    }
    
    double g = global_gain_.load();
    for (size_t i = 0; i < count * 2; ++i) {
        out_iq[i] *= g;
    }
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

bool StimulusManager::isAnyWithin(double center_hz, double bandwidth_hz) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [name, entry] : stimuli_) {
        if (entry.enabled && entry.stimulus) {
            double f = entry.stimulus->carrierFrequency();
            if (f == 0) continue; // Skip wideband noise
            if (std::abs(f - center_hz) <= bandwidth_hz / 2.0) {
                return true;
            }
        }
    }
    return false;
}

} // namespace nexrx
