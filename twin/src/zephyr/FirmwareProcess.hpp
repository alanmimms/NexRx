// NexRx Digital Twin - Firmware Process Manager
//
// Manages the Zephyr native_sim firmware process.
// Launches, monitors, and communicates with the firmware.
//
// Copyright 2026 NexRx Project - MIT License

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>

namespace nexrx {

//======================================================================
// Firmware Process Configuration
//======================================================================
struct FirmwareConfig {
    std::string executablePath;              // Path to zephyr.exe
    std::string shmName = "/nexrx_iq";       // Shared memory for I/Q
    std::string rpcSocketPath = "/tmp/nexrx_rpc.sock";  // RPC socket
    bool verbose = false;
};

//======================================================================
// Firmware Process Manager
//
// Launches and manages the Zephyr native_sim process.
// The firmware communicates with the twin via:
// - Shared memory for I/Q data (read by firmware)
// - Unix socket for RPC control (firmware sends commands)
//======================================================================
class FirmwareProcess {
public:
    using OutputCallback = std::function<void(const std::string&)>;

    FirmwareProcess() = default;
    ~FirmwareProcess();

    // Non-copyable
    FirmwareProcess(const FirmwareProcess&) = delete;
    FirmwareProcess& operator=(const FirmwareProcess&) = delete;

    // Launch the firmware process
    bool start(const FirmwareConfig& config);

    // Stop the firmware process
    void stop();

    // Check if process is running
    bool isRunning() const { return running_; }

    // Get process ID
    pid_t pid() const { return pid_; }

    // Set callback for firmware stdout/stderr
    void setOutputCallback(OutputCallback callback) {
        outputCallback_ = std::move(callback);
    }

    // Send a command to firmware via RPC
    bool sendCommand(const std::string& cmd);

    // Wait for firmware to exit (with timeout)
    // Returns true if exited, false on timeout
    bool waitForExit(int timeout_ms = 5000);

    // Get exit code (only valid after process exits)
    int exitCode() const { return exitCode_; }

private:
    void monitorThread();
    void setupRpcServer();

    FirmwareConfig config_;
    pid_t pid_ = -1;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};
    int exitCode_ = 0;

    // Pipes for stdout/stderr capture
    int stdout_pipe_[2] = {-1, -1};
    int stderr_pipe_[2] = {-1, -1};

    // RPC server socket
    int rpc_server_fd_ = -1;
    int rpc_client_fd_ = -1;

    std::thread monitorThread_;
    OutputCallback outputCallback_;
};

} // namespace nexrx
