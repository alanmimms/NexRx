#pragma once

#include "xyce/XyceWrapper.hpp"
#include "stimulus/StimulusManager.hpp"
#include <memory>
#include <string>
#include <functional>
#include <atomic>
#include <chrono>
#include <optional>

namespace nexrx {

struct OrchestratorConfig {
    std::string netlistPath;
    double simulationTimeStepNS = 1.0;
    double adcSampleRateHz = 96000.0;
    bool realTimeMode = false;
    bool verbose = false;
    std::string stimulusSourceName = "V_STIM";
    double stimulusBatchSizeUS = 10.0;
};

struct SimulationStats {
    uint64_t totalSteps = 0;
    uint64_t adcSamplesGenerated = 0;
    double simulatedTimeS = 0.0;
    double wallClockTimeS = 0.0;
    double speedFactor = 0.0;
};

class Orchestrator {
public:
    using ADCSampleCallback = std::function<void(double timeS, uint64_t sampleIndex)>;

    Orchestrator();
    ~Orchestrator();

    Orchestrator(const Orchestrator&) = delete;
    Orchestrator& operator=(const Orchestrator&) = delete;

    bool initialize(const OrchestratorConfig& config);
    [[nodiscard]] bool isInitialized() const noexcept { return initialized; }

    bool runFor(double durationS);
    bool runToCompletion();
    void stop();
    [[nodiscard]] bool isRunning() const noexcept { return running.load(); }
    [[nodiscard]] double getCurrentTime() const;
    [[nodiscard]] const SimulationStats& getStats() const noexcept { return stats; }

    void setADCSampleCallback(ADCSampleCallback callback);
    void setStimulusManager(std::shared_ptr<StimulusManager> manager);
    [[nodiscard]] std::shared_ptr<StimulusManager> getStimulusManager() const {
        return stimulusManager;
    }

    [[nodiscard]] std::optional<double> getNodeVoltage(const std::string& nodeName) const;
    bool setDeviceParam(const std::string& paramName, double value);
    [[nodiscard]] const OrchestratorConfig& getConfig() const noexcept { return config; }

private:
    OrchestratorConfig config;
    XyceWrapper xyce;
    bool initialized = false;
    std::atomic<bool> running{false};
    std::atomic<bool> stopRequested{false};

    SimulationStats stats;
    ADCSampleCallback adcCallback;

    double adcSamplePeriodS = 0.0;
    double nextADCSampleTimeS = 0.0;
    uint64_t adcSampleIndex = 0;

    double doStep(double targetTimeS);
    void checkADCSample(double currentTimeS);
    void updateStimulusData(double startTimeS, double endTimeS);

    std::shared_ptr<StimulusManager> stimulusManager;
    double nextStimulusUpdateTimeS = 0.0;
    double stimulusBatchPeriodS = 10e-6;
    bool stimulusEnabled = false;
};

} // namespace nexrx
