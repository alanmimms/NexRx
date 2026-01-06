/**
 * @file XyceWrapper.cpp
 * @brief Implementation of Xyce wrapper for Digital Twin
 *
 * @copyright 2026 NexRx Project - MIT License
 */

#include "xyce/XyceWrapper.hpp"

// Xyce headers
#include <N_CIR_Xyce.h>

#include <iostream>
#include <sstream>
#include <map>
#include <utility>

namespace NexRx::Twin {

XyceWrapper::XyceWrapper() = default;

XyceWrapper::~XyceWrapper() {
    finalize();
}

XyceWrapper::XyceWrapper(XyceWrapper&& other) noexcept
    : simulator_(std::move(other.simulator_))
    , initialized_(other.initialized_)
    , simMode_(other.simMode_)
    , lastError_(std::move(other.lastError_))
{
    other.initialized_ = false;
}

XyceWrapper& XyceWrapper::operator=(XyceWrapper&& other) noexcept {
    if (this != &other) {
        finalize();
        simulator_ = std::move(other.simulator_);
        initialized_ = other.initialized_;
        simMode_ = other.simMode_;
        lastError_ = std::move(other.lastError_);
        other.initialized_ = false;
    }
    return *this;
}

XyceWrapper::Status XyceWrapper::setError(Status status, const std::string& msg) {
    lastError_ = msg;
    std::cerr << "[XyceWrapper] Error: " << msg << std::endl;
    return status;
}

XyceWrapper::Status XyceWrapper::initialize(const std::string& netlistPath) {
    if (initialized_) {
        finalize();
    }

    // Create Xyce simulator instance
    simulator_ = std::make_unique<Xyce::Circuit::Simulator>();

    // Build argc/argv for Xyce initialization
    // Xyce expects: program_name netlist_file
    std::vector<char*> argv;
    std::string progName = "nexrx_twin";
    argv.push_back(const_cast<char*>(progName.c_str()));
    argv.push_back(const_cast<char*>(netlistPath.c_str()));

    // Initialize Xyce with the netlist
    auto result = simulator_->initialize(static_cast<int>(argv.size()), argv.data());

    if (result == Xyce::Circuit::Simulator::RunStatus::ERROR) {
        return setError(Status::InitError, "Xyce initialization failed for: " + netlistPath);
    }

    initialized_ = true;
    lastError_.clear();

    std::cout << "[XyceWrapper] Initialized with netlist: " << netlistPath << std::endl;
    return Status::Success;
}

std::optional<double> XyceWrapper::stepTo(double targetTimeSeconds) {
    if (!initialized_ || !simulator_) {
        lastError_ = "Simulator not initialized";
        return std::nullopt;
    }

    double completedTime = 0.0;
    bool success = simulator_->simulateUntil(targetTimeSeconds, completedTime);

    if (!success) {
        lastError_ = "simulateUntil failed";
        return std::nullopt;
    }

    return completedTime;
}

std::optional<double> XyceWrapper::getCurrentTime() const {
    if (!initialized_ || !simulator_) {
        return std::nullopt;
    }
    return simulator_->getTime();
}

std::optional<double> XyceWrapper::getFinalTime() const {
    if (!initialized_ || !simulator_) {
        return std::nullopt;
    }
    return simulator_->getFinalTime();
}

bool XyceWrapper::isSimulationComplete() const {
    if (!initialized_ || !simulator_) {
        return true; // Not initialized means "done" in a sense
    }
    return simulator_->simulationComplete();
}

std::optional<double> XyceWrapper::getNodeVoltage(const std::string& nodeName) const {
    if (!initialized_ || !simulator_) {
        lastError_ = "Simulator not initialized";
        return std::nullopt;
    }

    double value = 0.0;
    // Xyce expects voltage nodes as "V(nodename)"
    std::string queryName = "V(" + nodeName + ")";

    if (simulator_->getCircuitValue(queryName, value)) {
        return value;
    }

    // Try without V() wrapper in case it's already formatted
    if (simulator_->getCircuitValue(nodeName, value)) {
        return value;
    }

    lastError_ = "Node not found: " + nodeName;
    return std::nullopt;
}

std::optional<double> XyceWrapper::getDeviceParam(const std::string& fullParamName) const {
    if (!initialized_ || !simulator_) {
        lastError_ = "Simulator not initialized";
        return std::nullopt;
    }

    double value = 0.0;
    if (simulator_->getDeviceParamVal(fullParamName, value)) {
        return value;
    }

    lastError_ = "Parameter not found: " + fullParamName;
    return std::nullopt;
}

XyceWrapper::Status XyceWrapper::setDeviceParam(const std::string& fullParamName, double value) {
    if (!initialized_ || !simulator_) {
        return setError(Status::NotInitialized, "Simulator not initialized");
    }

    if (!simulator_->checkCircuitParameterExists(fullParamName)) {
        return setError(Status::NotFound, "Parameter not found: " + fullParamName);
    }

    if (!simulator_->setCircuitParameter(fullParamName, value)) {
        return setError(Status::SimError, "Failed to set parameter: " + fullParamName);
    }

    return Status::Success;
}

std::vector<std::string> XyceWrapper::getAllDeviceNames() const {
    std::vector<std::string> names;

    if (!initialized_ || !simulator_) {
        return names;
    }

    simulator_->getAllDeviceNames(names);
    return names;
}

bool XyceWrapper::updateTimeVoltagePairs(const std::string& sourceName,
                                         const std::vector<double>& times,
                                         const std::vector<double>& voltages) {
    if (!initialized_ || !simulator_) {
        lastError_ = "Simulator not initialized";
        return false;
    }

    if (times.size() != voltages.size() || times.empty()) {
        lastError_ = "Invalid time-voltage pair data";
        return false;
    }

    // Xyce API expects: map<sourceName, vector<pair<time,voltage>>*>
    std::vector<std::pair<double, double>> tvPairs;
    tvPairs.reserve(times.size());

    for (size_t i = 0; i < times.size(); ++i) {
        tvPairs.emplace_back(times[i], voltages[i]);
    }

    std::map<std::string, std::vector<std::pair<double, double>>*> updateMap;
    updateMap[sourceName] = &tvPairs;

    bool result = simulator_->updateTimeVoltagePairs(updateMap);

    if (!result) {
        lastError_ = "Failed to update time-voltage pairs for: " + sourceName;
    }

    return result;
}

void XyceWrapper::finalize() {
    if (simulator_ && initialized_) {
        simulator_->finalize();
        std::cout << "[XyceWrapper] Finalized" << std::endl;
    }
    simulator_.reset();
    initialized_ = false;
}

} // namespace NexRx::Twin
