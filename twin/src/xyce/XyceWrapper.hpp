/**
 * @file XyceWrapper.hpp
 * @brief C++ wrapper around Xyce Circuit Simulator for Digital Twin
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

namespace nexrx {

/**
 * @brief Wrapper class for Xyce circuit simulator
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

  XyceWrapper();
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
   * @return true if successful
   */
  bool initialize(const std::string& netlistPath);

  /**
   * @brief Check if simulator is initialized and ready
   */
  [[nodiscard]] bool isInitialized() const noexcept { return initialized; }

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
  [[nodiscard]] bool isFinished() const;

  /**
   * @brief Get voltage at a circuit node
   * @param nodeName Name of the node (as in netlist)
   * @return Voltage value, or nullopt if node not found
   */
  [[nodiscard]] std::optional<double> getNodeVoltage(const std::string& nodeName) const;

  /**
   * @brief Set a device parameter value
   * @param fullParamName Full parameter name (e.g., "R1:R")
   * @param value New parameter value
   * @return true if successful
   */
  bool setDeviceParameter(const std::string& fullParamName, double value);

  /**
   * @brief Update time-voltage pairs for an external data source
   */
  bool updateTimeVoltagePairs(const std::string& sourceName,
                              const std::vector<double>& times,
                              const std::vector<double>& voltages);

  /**
   * @brief Finalize and clean up the simulator
   */
  void finalize();

  void setSimMode(SimMode mode) { simMode = mode; }
  [[nodiscard]] SimMode getSimMode() const noexcept { return simMode; }
  [[nodiscard]] const std::string& getLastError() const noexcept { return lastError; }

private:
  std::unique_ptr<Xyce::Circuit::Simulator> simulator;
  bool initialized = false;
  SimMode simMode = SimMode::Accuracy;
  mutable std::string lastError;

  Status setError(Status status, const std::string& msg);
};

} // namespace nexrx
