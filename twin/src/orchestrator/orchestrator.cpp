#include "orchestrator.hpp"
#include <iostream>
#include <cmath>

namespace nexrx {

Orchestrator::Orchestrator() {}
Orchestrator::~Orchestrator() { stop(); }

bool Orchestrator::initialize(const OrchestratorConfig& cfg) {
    config = cfg;
    if (!xyce.initialize(config.netlistPath)) return false;
    
    adcSamplePeriodS = 1.0 / config.adcSampleRateHz;
    nextADCSampleTimeS = 0.0;
    adcSampleIndex = 0;
    
    initialized = true;
    return true;
}

bool Orchestrator::runFor(double durationS) {
    if (!initialized) return false;
    running.store(true);
    double endTime = xyce.getCurrentTime() + durationS;
    while (running.load() && xyce.getCurrentTime() < endTime) {
        doStep(endTime);
    }
    running.store(false);
    return true;
}

bool Orchestrator::runToCompletion() {
    if (!initialized) return false;
    running.store(true);
    while (running.load() && !xyce.isFinished()) {
        doStep(1e9); /* Large target */
    }
    running.store(false);
    return true;
}

void Orchestrator::stop() {
    running.store(false);
}

double Orchestrator::getCurrentTime() const {
    return xyce.getCurrentTime();
}

void Orchestrator::setADCSampleCallback(ADCSampleCallback callback) {
    adcCallback = std::move(callback);
}

void Orchestrator::setStimulusManager(std::shared_ptr<StimulusManager> manager) {
    stimulusManager = std::move(manager);
}

std::optional<double> Orchestrator::getNodeVoltage(const std::string& nodeName) const {
    return xyce.getNodeVoltage(nodeName);
}

bool Orchestrator::setDeviceParam(const std::string& paramName, double value) {
    return xyce.setDeviceParameter(paramName, value);
}

double Orchestrator::doStep(double targetTimeS) {
    double timeReached = xyce.step(targetTimeS);
    checkADCSample(timeReached);
    stats.totalSteps++;
    stats.simulatedTimeS = timeReached;
    return timeReached;
}

void Orchestrator::checkADCSample(double currentTimeS) {
    while (currentTimeS >= nextADCSampleTimeS) {
        if (adcCallback) adcCallback(nextADCSampleTimeS, adcSampleIndex);
        nextADCSampleTimeS += adcSamplePeriodS;
        adcSampleIndex++;
        stats.adcSamplesGenerated++;
    }
}

void Orchestrator::updateStimulusData(double startTimeS, double endTimeS) {
    /* TODO: Implement stimulus injection into Xyce */
}

} // namespace nexrx
