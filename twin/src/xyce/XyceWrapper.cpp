/**
 * @file XyceWrapper.cpp
 * @brief Implementation of Xyce wrapper for Digital Twin
 */

#include "xyce/XyceWrapper.hpp"

// Xyce headers
#include <N_CIR_Xyce.h>

#include <iostream>
#include <sstream>
#include <map>
#include <utility>

namespace nexrx {

XyceWrapper::XyceWrapper() = default;

XyceWrapper::~XyceWrapper() {
  finalize();
}

XyceWrapper::XyceWrapper(XyceWrapper&& other) noexcept
  : simulator(std::move(other.simulator))
  , initialized(other.initialized)
  , simMode(other.simMode)
  , lastError(std::move(other.lastError)) {
  other.initialized = false;
}

XyceWrapper& XyceWrapper::operator=(XyceWrapper&& other) noexcept {
  if (this != &other) {
    finalize();
    simulator = std::move(other.simulator);
    initialized = other.initialized;
    simMode = other.simMode;
    lastError = std::move(other.lastError);
    other.initialized = false;
  }
  return *this;
}

XyceWrapper::Status XyceWrapper::setError(Status status, const std::string& msg) {
  lastError = msg;
  std::cerr << "[XyceWrapper] Error: " << msg << std::endl;
  return status;
}

bool XyceWrapper::initialize(const std::string& netlistPath) {
  if (initialized) {
    finalize();
  }

  simulator = std::make_unique<Xyce::Circuit::Simulator>();

  std::vector<char*> argv;
  std::string progName = "nexrx_twin";
  argv.push_back(const_cast<char*>(progName.c_str()));
  argv.push_back(const_cast<char*>(netlistPath.c_str()));

  auto result = simulator->initialize(static_cast<int>(argv.size()), argv.data());

  if (result == Xyce::Circuit::Simulator::RunStatus::ERROR) {
    setError(Status::InitError, "Xyce initialization failed for: " + netlistPath);
    return false;
  }

  initialized = true;
  lastError.clear();

  std::cout << "[XyceWrapper] Initialized with netlist: " << netlistPath << std::endl;
  return true;
}

std::optional<double> XyceWrapper::stepTo(double targetTimeSeconds) {
  if (!initialized || !simulator) {
    lastError = "Simulator not initialized";
    return std::nullopt;
  }

  double completedTime = 0.0;
  bool success = simulator->simulateUntil(targetTimeSeconds, completedTime);

  if (!success) {
    lastError = "simulateUntil failed";
    return std::nullopt;
  }

  return completedTime;
}

std::optional<double> XyceWrapper::getCurrentTime() const {
  if (!initialized || !simulator) {
    return std::nullopt;
  }
  return simulator->getTime();
}

std::optional<double> XyceWrapper::getFinalTime() const {
  if (!initialized || !simulator) {
    return std::nullopt;
  }
  return simulator->getFinalTime();
}

bool XyceWrapper::isFinished() const {
  if (!initialized || !simulator) {
    return true; 
  }
  return simulator->simulationComplete();
}

std::optional<double> XyceWrapper::getNodeVoltage(const std::string& nodeName) const {
  if (!initialized || !simulator) {
    lastError = "Simulator not initialized";
    return std::nullopt;
  }

  double value = 0.0;
  std::string queryName = "V(" + nodeName + ")";

  if (simulator->getCircuitValue(queryName, value)) {
    return value;
  }

  if (simulator->getCircuitValue(nodeName, value)) {
    return value;
  }

  lastError = "Node not found: " + nodeName;
  return std::nullopt;
}

bool XyceWrapper::setDeviceParameter(const std::string& fullParamName, double value) {
  if (!initialized || !simulator) {
    return false;
  }

  if (!simulator->checkCircuitParameterExists(fullParamName)) {
    return false;
  }

  return simulator->setCircuitParameter(fullParamName, value);
}

bool XyceWrapper::updateTimeVoltagePairs(const std::string& sourceName,
                                         const std::vector<double>& times,
                                         const std::vector<double>& voltages) {
  if (!initialized || !simulator) {
    lastError = "Simulator not initialized";
    return false;
  }

  if (times.size() != voltages.size() || times.empty()) {
    lastError = "Invalid time-voltage pair data";
    return false;
  }

  std::vector<std::pair<double, double>> tvPairs;
  tvPairs.reserve(times.size());

  for (size_t i = 0; i < times.size(); ++i) {
    tvPairs.emplace_back(times[i], voltages[i]);
  }

  std::map<std::string, std::vector<std::pair<double, double>>*> updateMap;
  updateMap[sourceName] = &tvPairs;

  bool result = simulator->updateTimeVoltagePairs(updateMap);

  if (!result) {
    lastError = "Failed to update time-voltage pairs for: " + sourceName;
  }

  return result;
}

void XyceWrapper::finalize() {
  if (simulator && initialized) {
    simulator->finalize();
    std::cout << "[XyceWrapper] Finalized" << std::endl;
  }
  simulator.reset();
  initialized = false;
}

} // namespace nexrx
