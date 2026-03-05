#include "Orchestrator.hpp"
#include <iostream>
#include <cmath>

namespace nexrx {

Orchestrator::Orchestrator() {}
Orchestrator::~Orchestrator() {
  stop();
}

bool Orchestrator::initialize(const OrchestratorConfig& cfg) {
  config = cfg;
  if (!xyce.initialize(config.netlistPath)) {
    return false;
  }
  
  adcSamplePeriodS = 1.0 / config.adcSampleRateHz;
  nextADCSampleTimeS = 0.0;
  adcSampleIndex = 0;
  
  stats.totalSteps = 0;
  stats.adcSamplesGenerated = 0;
  stats.simTimeS = 0.0;
  stats.wallTimeS = 0.0;
  stats.speedFactor = 0.0;

  return true;
}

bool Orchestrator::runFor(double durationS) {
  running.store(true);
  auto startTime = std::chrono::steady_clock::now();
  double targetTime = xyce.getCurrentTime().value_or(0.0) + durationS;

  while (running.load() && xyce.getCurrentTime().value_or(0.0) < targetTime) {
    xyce.stepTo(targetTime);
    double currentTime = xyce.getCurrentTime().value_or(0.0);
    
    while (currentTime >= nextADCSampleTimeS) {
      if (adcCallback) {
        adcCallback(nextADCSampleTimeS, {}); // Voltages empty for now
      }
      nextADCSampleTimeS += adcSamplePeriodS;
      adcSampleIndex++;
      stats.adcSamplesGenerated++;
    }
    stats.totalSteps++;
    stats.simTimeS = currentTime;
  }

  auto endTime = std::chrono::steady_clock::now();
  std::chrono::duration<double> wallTime = endTime - startTime;
  stats.wallTimeS = wallTime.count();
  if (stats.wallTimeS > 0) {
    stats.speedFactor = stats.simTimeS / stats.wallTimeS;
  }

  running.store(false);
  return true;
}

bool Orchestrator::runToCompletion() {
  running.store(true);
  return runFor(1e9); // Just run until Xyce finished or stopped
}

void Orchestrator::start() {
  if (running.load()) return;
  workerThread = std::thread(&Orchestrator::run, this);
}

void Orchestrator::stop() {
  running.store(false);
  if (workerThread.joinable()) {
    workerThread.join();
  }
}

void Orchestrator::run() {
  runToCompletion();
}

std::optional<double> Orchestrator::getCurrentTimeS() const {
  return xyce.getCurrentTime();
}

std::optional<double> Orchestrator::getNodeVoltage(const std::string& nodeName) const {
  return xyce.getNodeVoltage(nodeName);
}

void Orchestrator::updateStimulusData(double startTimeS, double endTimeS) {
  (void)startTimeS;
  (void)endTimeS;
}

} // namespace nexrx
