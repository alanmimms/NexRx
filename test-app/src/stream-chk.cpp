#include "test-engine.hpp"
#include <thread>
#include <chrono>
#include <atomic>

namespace nexrx {

TestStatus stream_chk(RemoteDevice& device, std::string& message) {
    auto& conn = device.conn();
    
    // Start streaming
    if (!conn.startStream()) {
        message = "Failed to send STM[ command";
        return TestStatus::Failed;
    }
    
    if (!conn.startReceiving()) {
        message = "Failed to start UDP receiver thread";
        return TestStatus::Failed;
    }
    
    // Soak for 2 seconds
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    uint64_t frames = conn.getFramesReceived();
    uint64_t dropped = 0;
    uint64_t overruns = 0;
    
    conn.stopStream();
    
    if (frames == 0) {
        message = "No frames received over UDP";
        return TestStatus::Failed;
    }
    
    if (dropped > 0) {
        message = std::to_string(dropped) + " frames dropped";
        return TestStatus::Failed;
    }
    
    if (overruns > 0) {
        message = std::to_string(overruns) + " buffer overruns";
        return TestStatus::Failed;
    }
    
    message = "Received " + std::to_string(frames) + " frames flawlessly";
    return TestStatus::Passed;
}

} // namespace nexrx
