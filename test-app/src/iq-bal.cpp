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

// Extract Cos/Sin coefficients for a real signal
static void computeRealCoefs(const std::vector<double>& samples, double freqHz, double fs, double& cosCoef, double& sinCoef) {
    double c = 0, s = 0;
    for (size_t n = 0; n < samples.size(); ++n) {
        double angle = 2.0 * M_PI * freqHz * (double)n / fs;
        c += samples[n] * std::cos(angle);
        s += samples[n] * std::sin(angle);
    }
    cosCoef = 2.0 * c / (double)samples.size();
    sinCoef = 2.0 * s / (double)samples.size();
}

TestStatus iq_bal(RemoteDevice& device, std::string& message) {
    auto& conn = device.conn();
    std::cout << "\n[IQ Bal] Measuring QSD Image Rejection & Deriving Errors..." << std::endl;
    
    conn.setAtten(3, true); conn.setAtten(6, true); conn.setAtten(12, true); conn.setAtten(24, true);
    conn.setPreselectorInd(0, true); // Short L1
    for (int i=0; i<11; ++i) conn.setPreselectorCap(i, (64 >> i) & 1); 
    conn.setPgaGain(20.0);
    
    conn.setIsgEnable(true);
    conn.setIsgFreq(14.205e6);
    conn.setQsdVfo(0, 14.200e6); conn.setQsdVfo(1, 14.200e6); conn.setQsdVfo(2, 14.200e6);
    conn.startStream();
    conn.startReceiving();

    bool allOk = true;
    double fs = 96000.0;
    double offset = 5000.0;

    std::cout << "Channel | Rejection | Derived Gain Err   | Derived Phase Err  " << std::endl;
    std::cout << "--------+-----------+--------------------+--------------------" << std::endl;

    for (int ch = 0; ch < 3; ++ch) {
        std::vector<double> i_samples, q_samples;
        auto callback = [&](const IQFrame& frame) {
            if (i_samples.size() < 8192) {
                i_samples.push_back((double)frame.qsd[ch].i);
                q_samples.push_back((double)frame.qsd[ch].q);
            }
        };
        conn.setFrameCallback(callback);
        for (int i=0; i<100 && i_samples.size() < 8192; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        conn.setFrameCallback(nullptr);

        if (i_samples.size() < 8192) {
            std::cout << std::setw(7) << ch << " | NO DATA" << std::endl;
            allOk = false; continue;
        }

        double ic, is, qc, qs;
        computeRealCoefs(i_samples, offset, fs, ic, is);
        computeRealCoefs(q_samples, offset, fs, qc, qs);

        double i_mag = std::sqrt(ic*ic + is*is);
        double q_mag = std::sqrt(qc*qc + qs*qs);
        
        if (i_mag < 1000.0) {
            std::cout << std::setw(7) << ch << " | SIGNAL TOO WEAK (" << i_mag << ")" << std::endl;
            allOk = false; continue;
        }

        // Gain Imbalance
        double g_err = q_mag / i_mag;
        double g_err_db = 20.0 * std::log10(g_err);

        // Phase Error: Difference from 90 degrees
        double i_phase = std::atan2(-is, ic);
        double q_phase = std::atan2(-qs, qc);
        
        // Relative phase: Q leads I by (q_phase - i_phase)
        double phi_err_deg = -((q_phase - i_phase) * (180.0 / M_PI) + 90.0);
        while (phi_err_deg > 180.0) phi_err_deg -= 360.0;
        while (phi_err_deg < -180.0) phi_err_deg += 360.0;

        // Image Rejection Ratio (IRR)
        double phi_rad = phi_err_deg * (M_PI / 180.0);
        double rej_num = 1.0 + g_err*g_err + 2.0*g_err*std::cos(phi_rad);
        double rej_den = 1.0 + g_err*g_err - 2.0*g_err*std::cos(phi_rad);
        double rej = 10.0 * std::log10(rej_num / std::max(1e-10, rej_den));

        std::cout << std::setw(7) << ch << " | "
                  << std::fixed << std::setprecision(1) << std::setw(7) << rej << " dBc | "
                  << std::setw(15) << std::setprecision(3) << g_err_db << " dB | "
                  << std::setw(15) << std::setprecision(2) << phi_err_deg << " deg" << std::endl;

        if (rej < 25.0) allOk = false; 
    }

    conn.setIsgEnable(false);
    conn.stopStream();
    if (allOk) { message = "Errors derived using real-component analysis"; return TestStatus::Passed; }
    return TestStatus::Failed;
}

} // namespace nexrx
