#pragma once

#include "xyce/XyceWrapper.hpp"
#include "stimulus/StimulusManager.hpp"
#include <memory>
#include <string>
#include <functional>
#include <atomic>
#include <chrono>
#include <optional>
#include <thread>
#include <vector>

namespace nexrx {

struct OrchestratorConfig {
  std::string netlistPath;
  double durationS = 1.0;
  double adcSampleRateHz = 96000.0;
  bool verbose = true;
  bool realTimeMode = false;
  double simulationTimeStepNS = 10.0;
};

struct OrchestratorStats {
  std::atomic<uint64_t> adcSamplesGenerated{0};
  std::atomic<uint64_t> totalSteps{0};
  std::atomic<double> wallTimeS{0.0};
  std::atomic<double> simTimeS{0.0};
  std::atomic<double> speedFactor{0.0};
};

class Orchestrator {
public:
  using ADCSampleCallback = std::function<void(double timeS, const std::vector<double>& voltages)>;

  Orchestrator();
  ~Orchestrator();

  bool initialize(const OrchestratorConfig& config);
  void start();
  void stop();
  bool isRunning() const { return running; }

  // Blocking run methods
  bool runFor(double durationS);
  bool runToCompletion();

  void setADCSampleCallback(ADCSampleCallback callback) {
    adcCallback = std::move(callback);
  }

  // Get current simulation state
  const OrchestratorStats& getStats() const { return stats; }
  std::optional<double> getCurrentTimeS() const;
  std::optional<double> getNodeVoltage(const std::string& nodeName) const;

  // Stimulus control
  std::shared_ptr<StimulusManager> getStimulusManager() { return stimulusManager; }

private:
  void run();
  void updateStimulusData(double startTimeS, double endTimeS);

  OrchestratorConfig config;
  OrchestratorStats stats;
  
  std::atomic<bool> running{false};
  std::thread workerThread;

  ADCSampleCallback adcCallback;
  double adcSamplePeriodS = 0.0;
  double nextADCSampleTimeS = 0.0;
  uint64_t adcSampleIndex = 0;

  XyceWrapper xyce;

  std::shared_ptr<StimulusManager> stimulusManager;
  double nextStimulusUpdateTimeS = 0.0;
  double stimulusBatchPeriodS = 10e-6;
  bool stimulusEnabled = false;
};

} // namespace nexrx
