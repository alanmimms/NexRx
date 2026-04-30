#include "test-engine.hpp"
#include <thread>
#include <chrono>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <vector>

namespace nexrx {

// Helper to measure RMS power on a specific channel
static double measureChannelRms(RemoteDevice& device, int channel, int durationMs) {
    auto& conn = device.conn();
    double sumSq = 0;
    uint64_t count = 0;
    auto callback = [&](const IQFrame& frame) {
        double i = (double)frame.qsd[channel].i;
        double q = (double)frame.qsd[channel].q;
        sumSq += (i * i + q * q);
        count++;
    };
    conn.setFrameCallback(callback);
    std::this_thread::sleep_for(std::chrono::milliseconds(durationMs));
    conn.setFrameCallback(nullptr);
    return (count == 0) ? 0 : std::sqrt(sumSq / (double)count);
}

TestStatus pga_chk(RemoteDevice& device, std::string& message) {
    auto& conn = device.conn();
    std::cout << "\n[PGA] Verifying global gain control (all stages)..." << std::endl;
    std::cout << "QSD Ch  | Gain 0dB | Gain 20dB | Status" << std::endl;
    std::cout << "--------+----------+-----------+--------" << std::endl;

    conn.setAtten(45);
    // Select 14MHz BPF (band 3)
    conn.setHpfBypass(false);
    conn.setBpfIndex(3);
    
    conn.setISGEnable(true);
    conn.setISGFreq(14.205e6);
    conn.setVFO(14.200e6, 0.0);
    conn.startStream();
    conn.startReceiving();

    bool allPassed = true;
    
    // Baseline: 0dB
    conn.setPGAGain(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    double p0[3];
    for (int ch=0; ch<3; ++ch) p0[ch] = measureChannelRms(device, ch, 50);

    // Boost: 20dB
    conn.setPGAGain(5);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    double p20[3];
    for (int ch=0; ch<3; ++ch) p20[ch] = measureChannelRms(device, ch, 50);

    for (int ch=0; ch<3; ++ch) {
        // Expect ~10x voltage increase for 20dB
        bool ok = (p20[ch] > p0[ch] * 8.0 && p20[ch] < p0[ch] * 12.0); 
        std::cout << std::setw(7) << ch << " | "
                  << std::fixed << std::setprecision(1) << std::setw(8) << p0[ch] << " | "
                  << std::setw(9) << p20[ch] << " | "
                  << (ok ? "PASSED" : "FAILED") << std::endl;
        
        if (!ok) allPassed = false;
    }

    conn.setPGAGain(0); // Reset
    conn.setISGEnable(false);
    conn.stopStream();

    if (allPassed) {
        message = "Global PGA gain control verified across all channels";
        return TestStatus::Passed;
    }
    message = "Global PGA gain failed verification";
    return TestStatus::Failed;
}

} // namespace nexrx
