/**
 * @file Orchestrator.cpp
 * @brief Implementation of the Digital Twin orchestrator
 *
 * @copyright 2026 NexRx Project - MIT License
 */

#include "orchestrator/Orchestrator.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <vector>

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

    // Initialize stimulus injection
    stimulusBatchPeriod_s_ = config_.stimulusBatchSize_us * 1e-6;
    nextStimulusUpdateTime_s_ = 0.0;
    stimulusEnabled_ = false;

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
    auto lastProgressTime = wallStart;
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

        // Update stimulus data if needed (before stepping)
        if (stimulusEnabled_ && *currentTime >= nextStimulusUpdateTime_s_) {
            double batchEnd = nextStimulusUpdateTime_s_ + stimulusBatchPeriod_s_;
            updateStimulusData(nextStimulusUpdateTime_s_, batchEnd);
            nextStimulusUpdateTime_s_ = batchEnd;
        }

        // Execute step
        double reached = doStep(stepTarget);
        stats_.totalSteps++;

        // Check for ADC sample trigger
        checkAdcSample(reached);

        // Progress reporting (every 2 seconds of wall time)
        if (config_.verbose) {
            auto wallNow = std::chrono::steady_clock::now();
            double sinceLast = std::chrono::duration<double>(wallNow - lastProgressTime).count();
            if (sinceLast >= 2.0) {
                double progress = (reached - simStartTime) / duration_s * 100.0;
                double wallElapsed = std::chrono::duration<double>(wallNow - wallStart).count();
                double simElapsed = reached - simStartTime;
                double speed = simElapsed / wallElapsed;
                double eta = (duration_s - simElapsed) / speed;

                std::cout << "\r[Orchestrator] Progress: " << std::fixed << std::setprecision(1)
                          << progress << "% | "
                          << simElapsed * 1e6 << " us simulated | "
                          << stats_.adcSamplesGenerated << " samples | "
                          << speed << "x speed | "
                          << "ETA: " << eta << "s     " << std::flush;
                lastProgressTime = wallNow;
            }
        }

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

    // Clear progress line
    if (config_.verbose) {
        std::cout << "\r" << std::string(80, ' ') << "\r" << std::flush;
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

void Orchestrator::setStimulusManager(std::shared_ptr<nexrx::StimulusManager> manager) {
    stimulusManager_ = std::move(manager);

    // Enable stimulus injection if we have a manager
    if (stimulusManager_ && initialized_) {
        stimulusEnabled_ = true;
        nextStimulusUpdateTime_s_ = 0.0;

        if (config_.verbose) {
            std::cout << "[Orchestrator] Stimulus manager attached ("
                      << stimulusManager_->count() << " stimuli)" << std::endl;
        }
    }
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

void Orchestrator::updateStimulusData(double startTime_s, double endTime_s) {
    if (!stimulusEnabled_ || !stimulusManager_ || stimulusManager_->count() == 0) {
        return;
    }

    // Generate samples for the time window
    // Use high sample rate to capture RF frequencies (at least 2x max freq)
    // For HF (30 MHz max), we need at least 60 MHz sample rate
    // But Xyce interpolates, so we can use a reasonable rate
    constexpr double sampleRate_Hz = 100e6;  // 100 MHz sample rate
    const double samplePeriod_s = 1.0 / sampleRate_Hz;

    // Calculate number of samples
    double duration_s = endTime_s - startTime_s;
    size_t numSamples = static_cast<size_t>(duration_s * sampleRate_Hz) + 2;

    // Generate time-voltage pairs
    std::vector<double> times;
    std::vector<double> voltages;
    times.reserve(numSamples);
    voltages.reserve(numSamples);

    for (size_t i = 0; i < numSamples; ++i) {
        double t = startTime_s + i * samplePeriod_s;
        if (t > endTime_s + samplePeriod_s) break;

        times.push_back(t);
        voltages.push_back(stimulusManager_->getSample(t));
    }

    // Update Xyce voltage source
    if (!times.empty()) {
        bool success = xyce_.updateTimeVoltagePairs(
            config_.stimulusSourceName, times, voltages);

        if (!success && config_.verbose) {
            std::cerr << "[Orchestrator] Failed to update stimulus data: "
                      << xyce_.getLastError() << std::endl;
            // Disable further attempts if source doesn't exist
            stimulusEnabled_ = false;
        }
    }
}

} // namespace NexRx::Twin
