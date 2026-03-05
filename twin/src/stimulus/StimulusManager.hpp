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
#include <cmath>

namespace nexrx {

//======================================================================
// Stimulus Info - metadata about a registered stimulus
//======================================================================
struct StimulusInfo {
  std::string name;
  std::string type;       // "tone", "morse", "ssb", "noise", etc.
  double frequencyHz;    // Primary frequency (0 for noise)
  double amplitudeV;     // Peak amplitude
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
                   double freqHz = 0.0, double amplitudeV = 0.0);

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
  double getSample(double timeS) const;

  // Get combined analytic RF I/Q from all stimuli (for functional simulation)
  // ONLY includes stimuli within +/- bandwidthHz/2 of centerHz.
  // This acts as a roofing filter to prevent aliasing when sampling at discrete rates.
  void getRfIQ(double timeS, double& outI, double& outQ, 
               double centerHz = 0, double bandwidthHz = 0) const;

  //------------------------------------------------------------------
  // High-performance batch generation (lock-free after freeze)
  //------------------------------------------------------------------

  // Freeze the stimulus list for lock-free access during streaming.
  // After calling freeze(), getRfIQFast() can be called without locks.
  // Call unfreeze() before modifying stimuli.
  void freeze();
  void unfreeze();
  bool isFrozen() const { return frozen.load(std::memory_order_relaxed); }

  // Lock-free version of getRfIQ - only valid after freeze()
  void getRfIQFast(double timeS, double& outI, double& outQ,
                    double centerHz = 0, double bandwidthHz = 0) const;

  // Batch generate RF I/Q samples (most efficient for streaming)
  // Generates 'count' samples starting at 'startTime' with given period.
  // Writes interleaved I/Q pairs to output buffer (must be 2*count doubles).
  void generateBatch(double startTime, double samplePeriod,
                     size_t count, double* outIQ) const;

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
  void setGlobalLeveldB(double levelDB) { 
    globalGain.store(std::pow(10.0, levelDB / 20.0)); 
  }
  double getGlobalGain() const { return globalGain.load(); }

  // Check if any active stimulus is within bandwidth of target frequency
  bool isAnyWithin(double centerHz, double bandwidthHz) const;

private:
  struct Entry {
    StimulusPtr stimulus;
    StimulusInfo info;
    bool enabled = true;
  };

  mutable std::mutex stimulusMutex;
  std::unordered_map<std::string, Entry> stimuli;

  // Frozen snapshot for lock-free access during streaming
  std::atomic<bool> frozen{false};
  std::vector<StimulusPtr> frozenStimuli;  // Enabled stimuli only
  std::atomic<double> globalGain{1.0};
};

} // namespace nexrx
