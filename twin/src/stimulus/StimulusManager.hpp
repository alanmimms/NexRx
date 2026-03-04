// NexRx Digital Twin - Stimulus Manager
//
// Manages multiple named antenna stimuli for runtime control.
// Provides thread-safe access for Xyce callback integration.
//
// Copyright 2026 NexRx Project - MIT License

#pragma once

#include "AntennaStimulus.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace nexrx {

//======================================================================
// Stimulus Info - metadata about a registered stimulus
//======================================================================
struct StimulusInfo {
    std::string name;
    std::string type;       // "tone", "morse", "ssb", "noise", etc.
    double frequency_hz;    // Primary frequency (0 for noise)
    double amplitude_v;     // Peak amplitude
    bool active;            // Currently generating signal
};

//======================================================================
// Stimulus Manager
//
// Manages a collection of named antenna stimuli that are summed
// together to form the composite antenna input signal.
//
// Thread-safe: getSample() can be called from Xyce callback thread
// while add/remove operations happen from control thread.
//======================================================================
class StimulusManager {
public:
    using StimulusPtr = std::shared_ptr<AntennaStimulus>;

    StimulusManager() = default;
    ~StimulusManager() = default;

    // Non-copyable (owns resources)
    StimulusManager(const StimulusManager&) = delete;
    StimulusManager& operator=(const StimulusManager&) = delete;

    //------------------------------------------------------------------
    // Stimulus management
    //------------------------------------------------------------------

    // Add a named stimulus (replaces if name exists)
    void addStimulus(const std::string& name, StimulusPtr stimulus,
                     const std::string& type = "unknown",
                     double freq_hz = 0.0, double amplitude_v = 0.0);

    // Remove a stimulus by name
    bool removeStimulus(const std::string& name);

    // Remove all stimuli
    void clearAll();

    // Check if stimulus exists
    bool hasStimulus(const std::string& name) const;

    // Get list of all stimulus names
    std::vector<std::string> listNames() const;

    // Get info about all stimuli
    std::vector<StimulusInfo> listInfo() const;

    // Get stimulus count
    size_t count() const;

    //------------------------------------------------------------------
    // Signal generation (thread-safe, called from Xyce)
    //------------------------------------------------------------------

    // Get combined sample from all stimuli at given time
    // This is the main callback for Xyce behavioral source
    double getSample(double time_s) const;

    // Get combined analytic RF I/Q from all stimuli (for functional simulation)
    // ONLY includes stimuli within +/- bandwidth_hz/2 of center_hz.
    // This acts as a roofing filter to prevent aliasing when sampling at discrete rates.
    void getRfIQ(double time_s, double& out_i, double& out_q, 
                 double center_hz = 0, double bandwidth_hz = 0) const;

    //------------------------------------------------------------------
    // High-performance batch generation (lock-free after freeze)
    //------------------------------------------------------------------

    // Freeze the stimulus list for lock-free access during streaming.
    // After calling freeze(), getRfIQ_fast() can be called without locks.
    // Call unfreeze() before modifying stimuli.
    void freeze();
    void unfreeze();
    bool isFrozen() const { return frozen_.load(std::memory_order_relaxed); }

    // Lock-free version of getRfIQ - only valid after freeze()
    void getRfIQ_fast(double time_s, double& out_i, double& out_q,
                      double center_hz = 0, double bandwidth_hz = 0) const;

    // Batch generate RF I/Q samples (most efficient for streaming)
    // Generates 'count' samples starting at 'start_time' with given period.
    // Writes interleaved I/Q pairs to output buffer (must be 2*count doubles).
    void generateBatch(double start_time, double sample_period,
                       size_t count, double* out_iq) const;

    //------------------------------------------------------------------
    // Control
    //------------------------------------------------------------------

    // Enable/disable a specific stimulus
    void setEnabled(const std::string& name, bool enabled);

    // Enable/disable all stimuli
    void setAllEnabled(bool enabled);

    // Reset all stimuli (restart playback, etc.)
    void resetAll();

    // Global signal level control for fading/AGC testing
    void setGlobalLeveldB(double level_db) { 
        global_gain_.store(std::pow(10.0, level_db / 20.0)); 
    }
    double getGlobalGain() const { return global_gain_.load(); }

    // Check if any active stimulus is within bandwidth of target frequency
    bool isAnyWithin(double center_hz, double bandwidth_hz) const;

private:
    struct Entry {
        StimulusPtr stimulus;
        StimulusInfo info;
        bool enabled = true;
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::string, Entry> stimuli_;

    // Frozen snapshot for lock-free access during streaming
    std::atomic<bool> frozen_{false};
    std::vector<StimulusPtr> frozenStimuli_;  // Enabled stimuli only
    std::atomic<double> global_gain_{1.0};
};

} // namespace nexrx
