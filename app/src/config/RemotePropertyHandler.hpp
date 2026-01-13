// NexRx Configuration System - Remote Property Handler Interface
//
// Abstracts TCP command sending/receiving for remote properties.
// Implementation wraps TcpControlClient.
//
// Copyright 2026 NexRx Project - MIT License

#pragma once

#include <optional>
#include <string>

namespace nexrx {

//======================================================================
// Remote Property Handler Interface
//
// Provides abstraction for sending commands to the twin and parsing
// responses. Implementation wraps the actual TCP transport.
//======================================================================
class RemotePropertyHandler {
public:
    virtual ~RemotePropertyHandler() = default;

    // Check if connection to twin is active
    virtual bool isConnected() const = 0;

    // Send a command and get response
    // Returns std::nullopt on failure
    virtual std::optional<std::string> sendCommand(const std::string& cmd) = 0;

    // Convenience methods for common patterns

    // Send SET command: "SET_NAME value\n", expect "OK" response
    bool setRemote(const std::string& name, double value) {
        std::string cmd = "SET_" + name + " " + std::to_string(value) + "\n";
        auto response = sendCommand(cmd);
        return response && response->find("OK") == 0;
    }

    bool setRemote(const std::string& name, int value) {
        std::string cmd = "SET_" + name + " " + std::to_string(value) + "\n";
        auto response = sendCommand(cmd);
        return response && response->find("OK") == 0;
    }

    bool setRemote(const std::string& name, const std::string& value) {
        std::string cmd = "SET_" + name + " " + value + "\n";
        auto response = sendCommand(cmd);
        return response && response->find("OK") == 0;
    }

    // Send GET command: "GET_NAME\n", parse response
    // Returns std::nullopt on failure
    std::optional<double> getRemoteNumber(const std::string& name) {
        std::string cmd = "GET_" + name + "\n";
        auto response = sendCommand(cmd);
        if (!response) return std::nullopt;

        // Parse "NAME value" response format
        try {
            size_t pos = response->find(' ');
            if (pos != std::string::npos) {
                return std::stod(response->substr(pos + 1));
            }
        } catch (...) {
            // Parse failed
        }
        return std::nullopt;
    }

    std::optional<std::string> getRemoteString(const std::string& name) {
        std::string cmd = "GET_" + name + "\n";
        auto response = sendCommand(cmd);
        if (!response) return std::nullopt;

        // Parse "NAME value" response format
        size_t pos = response->find(' ');
        if (pos != std::string::npos) {
            return response->substr(pos + 1);
        }
        return std::nullopt;
    }
};

//======================================================================
// Null Remote Handler (for testing/offline mode)
//======================================================================
class NullRemoteHandler : public RemotePropertyHandler {
public:
    bool isConnected() const override { return false; }

    std::optional<std::string> sendCommand(const std::string&) override {
        return std::nullopt;
    }
};

} // namespace nexrx
