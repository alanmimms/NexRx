/**
 * @file Orchestrator.cpp
 * @brief Implementation of the Digital Twin orchestrator
 *
 * @copyright 2026 NexRx Project - MIT License
 */

#include "orchestrator/Orchestrator.hpp"
#include <iostream>
#include <chrono>
#include <thread>

namespace NexRx::Twin {

Orchestrator::Orchestrator() = default;

Orchestrator::~Orchestrator() {
    stop();
}

bool Orchestrator::initialize(const OrchestratorConfig& config) {
    config_ = config;

    // Initialize Xyce with the netlist
    auto status = xyce_.initialize(config_.netlistPath);
    if (status != XyceWrapper::Status::Success) {
        std::cerr << "[Orchestrator] Failed to initialize Xyce: "
                  << xyce_.getLastError() << std::endl;
        return false;
    }

    // Set simulation mode based on config
    xyce_.setSimMode(config_.realTimeMode ?
                     XyceWrapper::SimMode::RealTime :
                     XyceWrapper::SimMode::Accuracy);

    // Calculate ADC sample period
    adcSamplePeriod_s_ = 1.0 / config_.adcSampleRate_Hz;
    nextAdcSampleTime_s_ = adcSamplePeriod_s_; // First sample after one period
    adcSampleIndex_ = 0;

    // Reset statistics
    stats_ = SimulationStats{};

    initialized_ = true;

    if (config_.verbose) {
        std::cout << "[Orchestrator] Initialized" << std::endl;
        std::cout << "  Netlist: " << config_.netlistPath << std::endl;
        std::cout << "  Time step: " << config_.simulationTimeStep_ns << " ns" << std::endl;
        std::cout << "  ADC rate: " << config_.adcSampleRate_Hz << " Hz" << std::endl;
        std::cout << "  ADC period: " << adcSamplePeriod_s_ * 1e6 << " us" << std::endl;
        auto finalTime = xyce_.getFinalTime();
        if (finalTime) {
            std::cout << "  Simulation end: " << *finalTime * 1e3 << " ms" << std::endl;
        }
    }

    return true;
}

bool Orchestrator::runFor(double duration_s) {
    if (!initialized_) {
        std::cerr << "[Orchestrator] Not initialized" << std::endl;
        return false;
    }

    auto currentTime = xyce_.getCurrentTime();
    if (!currentTime) {
        std::cerr << "[Orchestrator] Could not get current time" << std::endl;
        return false;
    }

    double targetTime = *currentTime + duration_s;
    running_ = true;
    stopRequested_ = false;

    auto wallStart = std::chrono::steady_clock::now();
    double simStartTime = *currentTime;

    // Main simulation loop
    while (!stopRequested_ && !xyce_.isSimulationComplete()) {
        currentTime = xyce_.getCurrentTime();
        if (!currentTime || *currentTime >= targetTime) {
            break;
        }

        // Calculate next step target
        double stepTarget = *currentTime + (config_.simulationTimeStep_ns * 1e-9);
        if (stepTarget > targetTime) {
            stepTarget = targetTime;
        }

        // Execute step
        double reached = doStep(stepTarget);
        stats_.totalSteps++;

        // Check for ADC sample trigger
        checkAdcSample(reached);

        // Real-time mode: throttle if running too fast
        if (config_.realTimeMode) {
            auto wallNow = std::chrono::steady_clock::now();
            double wallElapsed = std::chrono::duration<double>(wallNow - wallStart).count();
            double simElapsed = reached - simStartTime;

            if (simElapsed > wallElapsed) {
                // Simulation ahead of real time - sleep
                auto sleepTime = std::chrono::duration<double>(simElapsed - wallElapsed);
                std::this_thread::sleep_for(sleepTime);
            }
        }
    }

    // Update statistics
    auto wallEnd = std::chrono::steady_clock::now();
    stats_.wallClockTime_s = std::chrono::duration<double>(wallEnd - wallStart).count();

    currentTime = xyce_.getCurrentTime();
    if (currentTime) {
        stats_.simulatedTime_s = *currentTime - simStartTime;
    }

    if (stats_.wallClockTime_s > 0) {
        stats_.speedFactor = stats_.simulatedTime_s / stats_.wallClockTime_s;
    }

    running_ = false;

    if (config_.verbose) {
        std::cout << "[Orchestrator] Run completed" << std::endl;
        std::cout << "  Steps: " << stats_.totalSteps << std::endl;
        std::cout << "  ADC samples: " << stats_.adcSamplesGenerated << std::endl;
        std::cout << "  Simulated: " << stats_.simulatedTime_s * 1e3 << " ms" << std::endl;
        std::cout << "  Wall clock: " << stats_.wallClockTime_s << " s" << std::endl;
        std::cout << "  Speed factor: " << stats_.speedFactor << "x" << std::endl;
    }

    return !stopRequested_;
}

bool Orchestrator::runToCompletion() {
    if (!initialized_) {
        return false;
    }

    auto finalTime = xyce_.getFinalTime();
    if (!finalTime) {
        std::cerr << "[Orchestrator] Could not determine final time" << std::endl;
        return false;
    }

    auto currentTime = xyce_.getCurrentTime();
    if (!currentTime) {
        return false;
    }

    double remaining = *finalTime - *currentTime;
    if (remaining <= 0) {
        return true; // Already complete
    }

    return runFor(remaining);
}

void Orchestrator::stop() {
    stopRequested_ = true;

    // Wait for running simulation to stop
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

double Orchestrator::getCurrentTime() const {
    auto time = xyce_.getCurrentTime();
    return time.value_or(0.0);
}

void Orchestrator::setAdcSampleCallback(AdcSampleCallback callback) {
    adcCallback_ = std::move(callback);
}

std::optional<double> Orchestrator::getNodeVoltage(const std::string& nodeName) const {
    return xyce_.getNodeVoltage(nodeName);
}

bool Orchestrator::setDeviceParam(const std::string& paramName, double value) {
    return xyce_.setDeviceParam(paramName, value) == XyceWrapper::Status::Success;
}

double Orchestrator::doStep(double targetTime_s) {
    auto result = xyce_.stepTo(targetTime_s);
    return result.value_or(targetTime_s);
}

void Orchestrator::checkAdcSample(double currentTime_s) {
    // Check if we've passed the next ADC sample time
    while (currentTime_s >= nextAdcSampleTime_s_) {
        // Trigger ADC sample callback
        if (adcCallback_) {
            adcCallback_(nextAdcSampleTime_s_, adcSampleIndex_);
        }

        stats_.adcSamplesGenerated++;
        adcSampleIndex_++;
        nextAdcSampleTime_s_ += adcSamplePeriod_s_;
    }
}

} // namespace NexRx::Twin
