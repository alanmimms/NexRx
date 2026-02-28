#include "remote-dev.hpp"
#include <iostream>

namespace nexrx {

bool RemoteDevice::connect(const std::string& host, int controlPort, int streamPort) {
    TwinConfig config;
    config.host = host;
    config.controlPort = static_cast<uint16_t>(controlPort);
    config.streamPort = static_cast<uint16_t>(streamPort);
    config.verbose = false;

    if (!conn_.initialize(config)) {
        return false;
    }

    // Basic check: get config
    std::string hwConfig = conn_.getHardwareConfig();
    if (hwConfig.empty() || hwConfig == "{}") {
        // Fallback for simulation if GCNF not fully implemented yet
    }

    return true;
}

void RemoteDevice::disconnect() {
    conn_.shutdown();
}

bool RemoteDevice::isConnected() const {
    return conn_.isConnected();
}

} // namespace nexrx
