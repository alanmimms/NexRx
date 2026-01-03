/**
 * @file Orchestrator.hpp
 * @brief Main simulation orchestrator for NexRx Digital Twin
 *
 * Coordinates Xyce simulation, NCO engine, ADC sampling, and
 * communication with the firmware surrogate.
 *
 * @copyright 2026 NexRx Project - MIT License
 */

#pragma once

#include "xyce/XyceWrapper.hpp"
#include <memory>
#include <string>
#include <functional>
#include <atomic>
#include <chrono>

namespace NexRx::Twin {

/**
 * @brief Configuration for the orchestrator
 */
struct OrchestratorConfig {
    std::string netlistPath;            ///< Path to main Xyce netlist
    double simulationTimeStep_ns = 1.0; ///< Base time step in nanoseconds
    double adcSampleRate_Hz = 96000.0;  ///< ADC sample rate (96 kHz)
    bool realTimeMode = false;          ///< Enable real-time mode
    bool verbose = false;               ///< Enable verbose logging
};

/**
 * @brief Statistics from simulation run
 */
struct SimulationStats {
    uint64_t totalSteps = 0;            ///< Total Xyce steps taken
    uint64_t adcSamplesGenerated = 0;   ///< Number of ADC samples produced
    double simulatedTime_s = 0.0;       ///< Total simulated time
    double wallClockTime_s = 0.0;       ///< Wall clock time elapsed
    double speedFactor = 0.0;           ///< Simulated/real time ratio
};

/**
 * @brief Main orchestrator class for Digital Twin simulation
 *
 * The Orchestrator is the central coordinator of the Digital Twin:
 * - Owns the Xyce simulation instance
 * - Controls simulation stepping
 * - Will coordinate NCO engine (Stage 2)
 * - Will generate ADC samples (Stage 6)
 * - Will communicate with firmware surrogate (Stage 5)
 */
class Orchestrator {
public:
    /**
     * @brief Callback type for each ADC sample interval
     * @param time_s Current simulation time in seconds
     * @param sampleIndex Sequential sample number
     */
    using AdcSampleCallback = std::function<void(double time_s, uint64_t sampleIndex)>;

    /**
     * @brief Constructor
     */
    Orchestrator();

    /**
     * @brief Destructor
     */
    ~Orchestrator();

    // Non-copyable, non-movable (owns resources)
    Orchestrator(const Orchestrator&) = delete;
    Orchestrator& operator=(const Orchestrator&) = delete;

    /**
     * @brief Initialize the orchestrator with configuration
     * @param config Configuration parameters
     * @return true if initialization successful
     */
    bool initialize(const OrchestratorConfig& config);

    /**
     * @brief Check if orchestrator is initialized
     */
    [[nodiscard]] bool isInitialized() const noexcept { return initialized_; }

    /**
     * @brief Run simulation for a specified duration
     * @param duration_s Duration in seconds to simulate
     * @return true if simulation completed successfully
     */
    bool runFor(double duration_s);

    /**
     * @brief Run simulation until completion (netlist .TRAN end time)
     * @return true if simulation completed successfully
     */
    bool runToCompletion();

    /**
     * @brief Stop a running simulation
     */
    void stop();

    /**
     * @brief Check if simulation is currently running
     */
    [[nodiscard]] bool isRunning() const noexcept { return running_.load(); }

    /**
     * @brief Get current simulation time
     * @return Time in seconds
     */
    [[nodiscard]] double getCurrentTime() const;

    /**
     * @brief Get simulation statistics
     */
    [[nodiscard]] const SimulationStats& getStats() const noexcept { return stats_; }

    /**
     * @brief Set callback for ADC sample events
     * @param callback Function to call at each ADC sample interval
     */
    void setAdcSampleCallback(AdcSampleCallback callback);

    /**
     * @brief Get a node voltage from the current simulation state
     * @param nodeName Node name
     * @return Voltage value, or nullopt if not found
     */
    [[nodiscard]] std::optional<double> getNodeVoltage(const std::string& nodeName) const;

    /**
     * @brief Set a device parameter in the simulation
     * @param paramName Full parameter name (e.g., "R1:R")
     * @param value New value
     * @return true if successful
     */
    bool setDeviceParam(const std::string& paramName, double value);

    /**
     * @brief Get configuration
     */
    [[nodiscard]] const OrchestratorConfig& getConfig() const noexcept { return config_; }

private:
    OrchestratorConfig config_;
    XyceWrapper xyce_;
    bool initialized_ = false;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};

    SimulationStats stats_;
    AdcSampleCallback adcCallback_;

    double adcSamplePeriod_s_ = 0.0;    ///< Period between ADC samples
    double nextAdcSampleTime_s_ = 0.0;  ///< Next scheduled ADC sample time
    uint64_t adcSampleIndex_ = 0;       ///< Current ADC sample counter

    /**
     * @brief Execute a single simulation step
     * @param targetTime_s Target time for this step
     * @return Actual time reached
     */
    double doStep(double targetTime_s);

    /**
     * @brief Check and trigger ADC sampling if needed
     * @param currentTime_s Current simulation time
     */
    void checkAdcSample(double currentTime_s);
};

} // namespace NexRx::Twin
