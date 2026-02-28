#include "test-engine.hpp"
#include <thread>
#include <chrono>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <complex>
#include <vector>
#include <algorithm>

namespace nexrx {

// Simple DFT at a single frequency point
static std::complex<double> computeDftPoint(const std::vector<std::pair<double, double>>& samples, double targetFreqHz, double fs) {
    std::complex<double> sum(0, 0);
    for (size_t n = 0; n < samples.size(); ++n) {
        double t = (double)n / fs;
        double angle = -2.0 * M_PI * targetFreqHz * t;
        std::complex<double> rotator(std::cos(angle), std::sin(angle));
        std::complex<double> sample(samples[n].first, samples[n].second);
        sum += sample * rotator;
    }
    return sum / (double)samples.size();
}

TestStatus iq_bal(RemoteDevice& device, std::string& message) {
    auto& conn = device.conn();
    std::cout << "\n[IQ Bal] Measuring QSD Image Rejection & Deriving Errors..." << std::endl;
    
    // Setup hardware state: Mask 64 (14.2MHz), 20dB Gain
    conn.setAtten(3, true); conn.setAtten(6, true); conn.setAtten(12, true); conn.setAtten(24, true);
    conn.setPreselectorInd(0, false); // Bypass L1
    for (int i=0; i<11; ++i) conn.setPreselectorCap(i, (64 >> i) & 1); 
    for (int i=0; i<6; ++i) conn.setPgaGain(i, 20.0);
    
    conn.setIsgEnable(true);
    conn.setIsgFreq(14.205e6);
    conn.setQsdVfo(0, 14.200e6); conn.setQsdVfo(1, 14.200e6); conn.setQsdVfo(2, 14.200e6);
    conn.startStream();
    conn.startReceiving();

    bool allOk = true;
    double fs = 96000.0;
    double offset = 5000.0; // ISG - VFO

    std::cout << "Channel | Rejection | Derived Gain Err | Derived Phase Err" << std::endl;
    std::cout << "--------+-----------+------------------+------------------" << std::endl;

    for (int ch = 0; ch < 3; ++ch) {
        std::vector<std::pair<double, double>> samples;
        auto callback = [&](const IQFrame& frame) {
            if (samples.size() < 8192) { // More samples for better resolution
                samples.push_back({(double)frame.qsd[ch].i, (double)frame.qsd[ch].q});
            }
        };
        conn.setFrameCallback(callback);
        // Wait up to 1s for samples
        for (int i=0; i<100 && samples.size() < 8192; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        conn.setFrameCallback(nullptr);

        if (samples.size() < 8192) {
            std::cout << std::setw(7) << ch << " | NO DATA" << std::endl;
            allOk = false;
            continue;
        }

        auto signal = computeDftPoint(samples, offset, fs);
        auto image = computeDftPoint(samples, -offset, fs);

        double sigMag = std::abs(signal);
        double imgMag = std::abs(image);
        
        if (sigMag < 100.0) {
            std::cout << std::setw(7) << ch << " | SIGNAL TOO WEAK (" << sigMag << ")" << std::endl;
            allOk = false;
            continue;
        }

        double rej = 20.0 * std::log10(sigMag / std::max(1.0, imgMag));

        // Derive Gain/Phase errors using the phasor ratio method
        // K = M / S* = (1 - g*exp(j*phi)) / (1 + g*exp(-j*phi))
        std::complex<double> K = image / std::conj(signal);
        
        // g*exp(j*phi) = (1 - K) / (1 + K*) -- exact for non-orthogonal error model
        std::complex<double> g_phi = (1.0 - K) / (1.0 + std::conj(K));
        
        double g_err_db = 20.0 * std::log10(std::abs(g_phi));
        double phi_err_deg = std::arg(g_phi) * (180.0 / M_PI);

        std::cout << std::setw(7) << ch << " | "
                  << std::fixed << std::setprecision(1) << std::setw(7) << rej << " dBc | "
                  << std::setw(13) << std::setprecision(3) << g_err_db << " dB | "
                  << std::setw(13) << std::setprecision(2) << -phi_err_deg << " deg" << std::endl;

        if (rej < 25.0) allOk = false; 
    }

    conn.setIsgEnable(false);
    conn.stopStream();

    if (allOk) {
        message = "Image rejection measured and errors derived";
        return TestStatus::Passed;
    }
    message = "Low image rejection or data loss detected";
    return TestStatus::Failed;
}

} // namespace nexrx
