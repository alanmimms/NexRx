/**
 * @file XyceWrapper.hpp
 * @brief C++ wrapper around Xyce Circuit Simulator for Digital Twin
 *
 * Provides a clean interface to the Xyce SPICE simulator, abstracting
 * the complexity of the Xyce API for use in the NexRx Digital Twin
 * orchestrator.
 *
 * @copyright 2026 NexRx Project - MIT License
 */

#pragma once

#include <memory>
#include <string>
#include <optional>
#include <vector>
#include <functional>

// Forward declare Xyce types to avoid header pollution
namespace Xyce::Circuit {
class Simulator;
}

namespace NexRx::Twin {

/**
 * @brief Wrapper class for Xyce circuit simulator
 *
 * Encapsulates a Xyce simulation instance, providing methods for:
 * - Loading and initializing netlists
 * - Stepping simulation forward in time
 * - Reading node voltages and device currents
 * - Setting device parameters dynamically
 */
class XyceWrapper {
public:
    /**
     * @brief Simulation status codes
     */
    enum class Status {
        Success,        ///< Operation completed successfully
        NotInitialized, ///< Simulator not yet initialized
        InitError,      ///< Initialization failed
        SimError,       ///< Simulation error occurred
        NotFound,       ///< Requested node/device not found
        InvalidParam    ///< Invalid parameter value
    };

    /**
     * @brief Simulation mode for speed/accuracy tradeoff
     */
    enum class SimMode {
        Accuracy,   ///< Full physics simulation (slower)
        RealTime    ///< Coarser stepping for real-time operation
    };

    /**
     * @brief Constructor
     */
    XyceWrapper();

    /**
     * @brief Destructor - ensures proper Xyce cleanup
     */
    ~XyceWrapper();

    // Non-copyable (owns Xyce simulator instance)
    XyceWrapper(const XyceWrapper&) = delete;
    XyceWrapper& operator=(const XyceWrapper&) = delete;

    // Movable
    XyceWrapper(XyceWrapper&&) noexcept;
    XyceWrapper& operator=(XyceWrapper&&) noexcept;

    /**
     * @brief Initialize simulator with a netlist file
     * @param netlistPath Path to the Xyce netlist (.cir) file
     * @return Status code
     */
    Status initialize(const std::string& netlistPath);

    /**
     * @brief Check if simulator is initialized and ready
     */
    [[nodiscard]] bool isInitialized() const noexcept { return initialized_; }

    /**
     * @brief Advance simulation to specified time
     * @param targetTimeSeconds Target simulation time in seconds
     * @return Actual time reached (may be less if simulation completes early)
     */
    std::optional<double> stepTo(double targetTimeSeconds);

    /**
     * @brief Get current simulation time
     * @return Current time in seconds, or nullopt if not initialized
     */
    [[nodiscard]] std::optional<double> getCurrentTime() const;

    /**
     * @brief Get final simulation time from netlist
     * @return Final time in seconds, or nullopt if not available
     */
    [[nodiscard]] std::optional<double> getFinalTime() const;

    /**
     * @brief Check if simulation has completed
     */
    [[nodiscard]] bool isSimulationComplete() const;

    /**
     * @brief Get voltage at a circuit node
     * @param nodeName Name of the node (as in netlist)
     * @return Voltage value, or nullopt if node not found
     */
    [[nodiscard]] std::optional<double> getNodeVoltage(const std::string& nodeName) const;

    /**
     * @brief Get a device parameter value
     * @param fullParamName Full parameter name (e.g., "R1:R" for resistance)
     * @return Parameter value, or nullopt if not found
     */
    [[nodiscard]] std::optional<double> getDeviceParam(const std::string& fullParamName) const;

    /**
     * @brief Set a device parameter value
     * @param fullParamName Full parameter name (e.g., "R1:R")
     * @param value New parameter value
     * @return Status code
     */
    Status setDeviceParam(const std::string& fullParamName, double value);

    /**
     * @brief Get list of all device names in the circuit
     * @return Vector of device names
     */
    [[nodiscard]] std::vector<std::string> getAllDeviceNames() const;

    /**
     * @brief Finalize and clean up the simulator
     */
    void finalize();

    /**
     * @brief Set simulation mode
     * @param mode Accuracy or RealTime mode
     */
    void setSimMode(SimMode mode) { simMode_ = mode; }

    /**
     * @brief Get current simulation mode
     */
    [[nodiscard]] SimMode getSimMode() const noexcept { return simMode_; }

    /**
     * @brief Get last error message
     */
    [[nodiscard]] const std::string& getLastError() const noexcept { return lastError_; }

private:
    std::unique_ptr<Xyce::Circuit::Simulator> simulator_;
    bool initialized_ = false;
    SimMode simMode_ = SimMode::Accuracy;
    mutable std::string lastError_;

    // Helper to set error message and return status
    Status setError(Status status, const std::string& msg);
};

} // namespace NexRx::Twin
