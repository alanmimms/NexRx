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
    conn.resetStats();
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

    conn.setAtten(3, true); conn.setAtten(6, true); conn.setAtten(12, true); conn.setAtten(24, true);
    // Tune preselector to ~14.2MHz (Mask 64, L1 Shorted/Bypassed)
    conn.setPreselectorInd(0, true);
    for (int i=0; i<11; ++i) conn.setPreselectorCap(i, (64 >> i) & 1);
    
    conn.setIsgEnable(true);
    conn.setIsgFreq(14.205e6);
    conn.setQsdVfo(0, 14.200e6); conn.setQsdVfo(1, 14.200e6); conn.setQsdVfo(2, 14.200e6);
    conn.startStream();
    conn.startReceiving();

    bool allPassed = true;
    
    // Baseline: 0dB
    conn.setPgaGain(0.0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    double p0[3];
    for (int ch=0; ch<3; ++ch) p0[ch] = measureChannelRms(device, ch, 50);

    // Boost: 20dB
    conn.setPgaGain(20.0);
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

    conn.setPgaGain(0.0); // Reset
    conn.setIsgEnable(false);
    conn.stopStream();

    if (allPassed) {
        message = "Global PGA gain control verified across all channels";
        return TestStatus::Passed;
    }
    message = "Global PGA gain failed verification";
    return TestStatus::Failed;
}

} // namespace nexrx
