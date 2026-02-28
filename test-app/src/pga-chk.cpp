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
    std::cout << "\n[PGA] Verifying 6 MAX9939 stages..." << std::endl;
    std::cout << "PGA Idx | Gain 0dB | Gain 20dB | Status" << std::endl;
    std::cout << "--------+----------+-----------+--------" << std::endl;

    conn.setAtten(3, true); conn.setAtten(6, true); conn.setAtten(12, true); conn.setAtten(24, true);
    // Tune preselector to ~14.2MHz (Mask 64, L1 Off)
    conn.setPreselectorInd(0, false);
    for (int i=0; i<11; ++i) conn.setPreselectorCap(i, (64 >> i) & 1);
    
    conn.setIsgEnable(true);
    conn.setIsgFreq(14.205e6);
    conn.setQsdVfo(0, 14.200e6); conn.setQsdVfo(1, 14.200e6); conn.setQsdVfo(2, 14.200e6);
    conn.startStream();
    conn.startReceiving();

    bool allPassed = true;
    for (int pga = 0; pga < 6; ++pga) {
        int qsdCh = pga / 2;
        
        // Reset all PGAs to 0dB baseline
        for (int i=0; i<6; ++i) conn.setPgaGain(i, 0.0);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        double p0 = measureChannelRms(device, qsdCh, 50);

        conn.setPgaGain(pga, 20.0);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        double p20 = measureChannelRms(device, qsdCh, 50);

        // Expect ~10x voltage increase for 20dB on one branch of a complex signal
        // sqrt(10^2 + 1^2) / sqrt(1^2 + 1^2) = 10.05 / 1.414 = 7.1x
        bool ok = (p20 > p0 * 4.0); 
        std::cout << std::setw(7) << pga << " | "
                  << std::fixed << std::setprecision(1) << std::setw(8) << p0 << " | "
                  << std::setw(9) << p20 << " | "
                  << (ok ? "PASSED" : "FAILED") << std::endl;
        
        if (!ok) allPassed = false;
        conn.setPgaGain(pga, 0.0); // Reset
    }

    conn.setIsgEnable(false);
    conn.stopStream();

    if (allPassed) {
        message = "All 6 PGA stages verified";
        return TestStatus::Passed;
    }
    message = "One or more PGA stages failed linearity check";
    return TestStatus::Failed;
}

} // namespace nexrx
