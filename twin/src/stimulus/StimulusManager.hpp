// NexRx Digital Twin - Stimulus Manager
//
// Manages multiple named antenna stimuli for runtime control.
// Provides thread-safe access for Xyce callback integration.
//
// Copyright 2026 NexRx Project - MIT License

#pragma once

#include "AntennaStimulus.hpp"

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

    // Get combined baseband I/Q from all stimuli (for functional simulation)
    // Computes baseband analytically to avoid RF aliasing at audio sample rates
    void getBasebandIQ(double time_s, double lo_freq_hz,
                       double& out_i, double& out_q) const;

    //------------------------------------------------------------------
    // Control
    //------------------------------------------------------------------

    // Enable/disable a specific stimulus
    void setEnabled(const std::string& name, bool enabled);

    // Enable/disable all stimuli
    void setAllEnabled(bool enabled);

    // Reset all stimuli (restart playback, etc.)
    void resetAll();

private:
    struct Entry {
        StimulusPtr stimulus;
        StimulusInfo info;
        bool enabled = true;
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::string, Entry> stimuli_;
};

} // namespace nexrx
