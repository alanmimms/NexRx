// NexRx Configuration System - Twin Remote Handler
//
// Concrete implementation of RemotePropertyHandler that wraps HostApp
// to send commands to the digital twin.
//
// Copyright 2026 NexRx Project - MIT License

#pragma once

#include "RemotePropertyHandler.hpp"
#include "twin/HostApp.hpp"

namespace nexrx {

class TwinRemoteHandler : public RemotePropertyHandler {
public:
    explicit TwinRemoteHandler(HostApp& host) : host_(host) {}

    bool isConnected() const override {
        return host_.isConnected();
    }

    std::optional<std::string> sendCommand(const std::string& cmd) override {
        if (!host_.isConnected()) {
            return std::nullopt;
        }

        std::string response = host_.sendCommand(cmd);
        if (response.empty()) {
            return std::nullopt;
        }

        return response;
    }

private:
    HostApp& host_;
};

} // namespace nexrx
