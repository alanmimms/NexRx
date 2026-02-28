#include "test-engine.hpp"
#include <thread>
#include <chrono>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <complex>
#include <vector>

namespace nexrx {

// Simple DFT at a single frequency point
static std::complex<double> computeDftPoint(const std::vector<std::pair<double, double>>& samples, double targetFreqHz, double fs) {
    std::complex<double> sum(0, 0);
    for (size_t n = 0; n < samples.size(); ++n) {
        double angle = -2.0 * M_PI * targetFreqHz * (double)n / fs;
        std::complex<double> rotator(std::cos(angle), std::sin(angle));
        std::complex<double> sample(samples[n].first, samples[n].second);
        sum += sample * rotator;
    }
    return sum / (double)samples.size();
}

TestStatus iq_bal(RemoteDevice& device, std::string& message) {
    auto& conn = device.conn();
    std::cout << "\n[IQ Bal] Measuring QSD Image Rejection..." << std::endl;
    std::cout << "Channel | Signal (dB) | Image (dB) | Rejection (dBc)" << std::endl;
    std::cout << "--------+-------------+------------+----------------" << std::endl;

    conn.setAtten(3, true); conn.setAtten(6, true); conn.setAtten(12, true); conn.setAtten(24, true);
    // Tune preselector to ~14.2MHz
    conn.setPreselectorInd(0, false);
    for (int i=0; i<11; ++i) conn.setPreselectorCap(i, (64 >> i) & 1);

    for (int i=0; i<6; ++i) conn.setPgaGain(i, 20.0);
    conn.setIsgEnable(true);
    conn.setIsgFreq(14.205e6);
    conn.setQsdVfo(0, 14.200e6); conn.setQsdVfo(1, 14.200e6); conn.setQsdVfo(2, 14.200e6);
    conn.startStream();
    conn.startReceiving();

    bool allOk = true;
    double fs = 96000.0;
    double offset = 5000.0; // 14.205 - 14.200

    for (int ch = 0; ch < 3; ++ch) {
        std::vector<std::pair<double, double>> samples;
        auto callback = [&](const IQFrame& frame) {
            if (samples.size() < 4096) {
                samples.push_back({(double)frame.qsd[ch].i, (double)frame.qsd[ch].q});
            }
        };
        conn.setFrameCallback(callback);
        // Wait up to 500ms for samples
        for (int i=0; i<50 && samples.size() < 4096; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        conn.setFrameCallback(nullptr);

        auto signal = computeDftPoint(samples, offset, fs);
        auto image = computeDftPoint(samples, -offset, fs);

        double sigP = 20.0 * std::log10(std::max(1.0, std::abs(signal)));
        double imgP = 20.0 * std::log10(std::max(1.0, std::abs(image)));
        double rej = sigP - imgP;

        std::cout << std::setw(7) << ch << " | "
                  << std::fixed << std::setprecision(1) << std::setw(11) << sigP << " | "
                  << std::setw(10) << imgP << " | "
                  << std::setw(14) << rej << std::endl;

        if (rej < 30.0) allOk = false; // Expected rejection for the simulated error is ~34dBc
    }

    conn.setIsgEnable(false);
    conn.stopStream();

    if (allOk) {
        message = "Uncalibrated image rejection within expected range for simulation";
        return TestStatus::Passed;
    }
    message = "Low image rejection detected";
    return TestStatus::Failed;
}

} // namespace nexrx
