// NexRx Digital Twin - Firmware Process Manager Implementation
//
// Copyright 2026 NexRx Project - MIT License

#include "FirmwareProcess.hpp"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <chrono>

#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

namespace nexrx {

FirmwareProcess::~FirmwareProcess() {
    stop();
}

bool FirmwareProcess::start(const FirmwareConfig& config) {
    if (running_) {
        return false;
    }

    config_ = config;
    stopRequested_ = false;

    // Set up RPC server socket before forking
    setupRpcServer();

    // Create pipes for stdout/stderr capture
    if (pipe(stdout_pipe_) < 0 || pipe(stderr_pipe_) < 0) {
        std::cerr << "[FirmwareProcess] Failed to create pipes" << std::endl;
        return false;
    }

    // Fork to create firmware process
    pid_ = fork();

    if (pid_ < 0) {
        std::cerr << "[FirmwareProcess] Fork failed: " << strerror(errno) << std::endl;
        return false;
    }

    if (pid_ == 0) {
        // Child process - exec the firmware

        // Redirect stdout/stderr to pipes
        dup2(stdout_pipe_[1], STDOUT_FILENO);
        dup2(stderr_pipe_[1], STDERR_FILENO);
        close(stdout_pipe_[0]);
        close(stdout_pipe_[1]);
        close(stderr_pipe_[0]);
        close(stderr_pipe_[1]);

        // Set environment variables for firmware
        setenv("NEXRX_SHM_NAME", config_.shmName.c_str(), 1);
        setenv("NEXRX_RPC_SOCKET", config_.rpcSocketPath.c_str(), 1);

        // Execute firmware
        execl(config_.executablePath.c_str(),
              config_.executablePath.c_str(),
              nullptr);

        // If we get here, exec failed
        std::cerr << "Failed to exec firmware: " << strerror(errno) << std::endl;
        _exit(127);
    }

    // Parent process
    close(stdout_pipe_[1]);
    close(stderr_pipe_[1]);

    // Set pipes to non-blocking
    fcntl(stdout_pipe_[0], F_SETFL, O_NONBLOCK);
    fcntl(stderr_pipe_[0], F_SETFL, O_NONBLOCK);

    running_ = true;

    // Start monitor thread
    monitorThread_ = std::thread(&FirmwareProcess::monitorThread, this);

    if (config_.verbose) {
        std::cout << "[FirmwareProcess] Started firmware process, PID: " << pid_ << std::endl;
    }

    return true;
}

void FirmwareProcess::stop() {
    if (!running_) {
        return;
    }

    stopRequested_ = true;

    // Send SIGTERM to firmware
    if (pid_ > 0) {
        kill(pid_, SIGTERM);

        // Wait briefly for graceful exit
        if (!waitForExit(1000)) {
            // Force kill
            kill(pid_, SIGKILL);
            waitForExit(1000);
        }
    }

    // Clean up
    if (monitorThread_.joinable()) {
        monitorThread_.join();
    }

    if (stdout_pipe_[0] >= 0) {
        close(stdout_pipe_[0]);
        stdout_pipe_[0] = -1;
    }
    if (stderr_pipe_[0] >= 0) {
        close(stderr_pipe_[0]);
        stderr_pipe_[0] = -1;
    }

    if (rpc_client_fd_ >= 0) {
        close(rpc_client_fd_);
        rpc_client_fd_ = -1;
    }
    if (rpc_server_fd_ >= 0) {
        close(rpc_server_fd_);
        rpc_server_fd_ = -1;
        unlink(config_.rpcSocketPath.c_str());
    }

    running_ = false;
    pid_ = -1;

    if (config_.verbose) {
        std::cout << "[FirmwareProcess] Stopped" << std::endl;
    }
}

bool FirmwareProcess::waitForExit(int timeout_ms) {
    auto start = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(timeout_ms);

    while (running_) {
        int status;
        pid_t result = waitpid(pid_, &status, WNOHANG);

        if (result == pid_) {
            // Process exited
            if (WIFEXITED(status)) {
                exitCode_ = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                exitCode_ = -WTERMSIG(status);
            }
            running_ = false;
            return true;
        }

        if (std::chrono::steady_clock::now() - start > timeout) {
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return true;
}

void FirmwareProcess::monitorThread() {
    char buf[1024];

    while (running_ && !stopRequested_) {
        // Check for process exit
        int status;
        pid_t result = waitpid(pid_, &status, WNOHANG);

        if (result == pid_) {
            if (WIFEXITED(status)) {
                exitCode_ = WEXITSTATUS(status);
                if (config_.verbose) {
                    std::cout << "[FirmwareProcess] Exited with code: " << exitCode_ << std::endl;
                }
            } else if (WIFSIGNALED(status)) {
                exitCode_ = -WTERMSIG(status);
                if (config_.verbose) {
                    std::cout << "[FirmwareProcess] Killed by signal: " << WTERMSIG(status) << std::endl;
                }
            }
            running_ = false;
            break;
        }

        // Read stdout
        ssize_t n = read(stdout_pipe_[0], buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            if (outputCallback_) {
                outputCallback_(std::string(buf, n));
            } else if (config_.verbose) {
                std::cout << "[FW] " << buf;
            }
        }

        // Read stderr
        n = read(stderr_pipe_[0], buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            if (outputCallback_) {
                outputCallback_(std::string("[ERR] ") + std::string(buf, n));
            } else if (config_.verbose) {
                std::cerr << "[FW ERR] " << buf;
            }
        }

        // Accept RPC connection if not already connected
        if (rpc_server_fd_ >= 0 && rpc_client_fd_ < 0) {
            struct sockaddr_un addr;
            socklen_t len = sizeof(addr);
            int fd = accept(rpc_server_fd_, (struct sockaddr*)&addr, &len);
            if (fd >= 0) {
                rpc_client_fd_ = fd;
                if (config_.verbose) {
                    std::cout << "[FirmwareProcess] RPC client connected" << std::endl;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void FirmwareProcess::setupRpcServer() {
    // Remove any existing socket file
    unlink(config_.rpcSocketPath.c_str());

    // Create server socket
    rpc_server_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (rpc_server_fd_ < 0) {
        std::cerr << "[FirmwareProcess] Failed to create RPC socket" << std::endl;
        return;
    }

    // Bind to path
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, config_.rpcSocketPath.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(rpc_server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[FirmwareProcess] Failed to bind RPC socket: "
                  << strerror(errno) << std::endl;
        close(rpc_server_fd_);
        rpc_server_fd_ = -1;
        return;
    }

    // Listen for connections
    listen(rpc_server_fd_, 1);

    // Set non-blocking
    fcntl(rpc_server_fd_, F_SETFL, O_NONBLOCK);

    if (config_.verbose) {
        std::cout << "[FirmwareProcess] RPC server listening on: "
                  << config_.rpcSocketPath << std::endl;
    }
}

bool FirmwareProcess::sendCommand(const std::string& cmd) {
    if (rpc_client_fd_ < 0) {
        return false;
    }

    std::string msg = cmd + "\n";
    ssize_t sent = send(rpc_client_fd_, msg.c_str(), msg.size(), 0);

    return sent == static_cast<ssize_t>(msg.size());
}

} // namespace nexrx
