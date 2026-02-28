#include "test-engine.hpp"
#include <thread>
#include <chrono>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <fstream>

namespace nexrx {

static double measureRms(RemoteDevice& device, int durationMs) {
    auto& conn = device.conn();
    conn.resetStats();
    double sumSq = 0;
    uint64_t count = 0;
    auto callback = [&](const IQFrame& frame) {
        double i = (double)frame.qsd[2].i;
        double q = (double)frame.qsd[2].q;
        sumSq += (i * i + q * q);
        count++;
    };
    conn.setFrameCallback(callback);
    std::this_thread::sleep_for(std::chrono::milliseconds(durationMs));
    conn.setFrameCallback(nullptr);
    if (count == 0) return 0;
    return std::sqrt(sumSq / (double)count);
}

struct FilterResponse {
    double peakFreq;
    double low3dB;
    double high3dB;
    double peakPower;
    double bw3dB;
};

static FilterResponse characterizeMask(RemoteDevice& device, uint32_t mask, bool useL1) {
    auto& conn = device.conn();
    for (int i=0; i<11; ++i) conn.setPreselectorCap(i, (mask >> i) & 1);
    conn.setPreselectorInd(0, useL1);
    
    double start = 100e3, stop = 80e6; 
    int steps = 100;
    double stepSize = std::pow(stop/start, 1.0/steps);
    
    double maxP = -1.0;
    double peakF = 0;
    for (int i=0; i<=steps; ++i) {
        double f = start * std::pow(stepSize, i);
        conn.setIsgFreq(f);
        conn.setQsdVfo(2, f);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        double p = measureRms(device, 10);
        if (p > maxP) { maxP = p; peakF = f; }
    }

    double span = std::min(10.0e6, peakF * 0.4); 
    double fStart = std::max(100e3, peakF - span/2);
    double fStop = peakF + span/2;
    int fineSteps = 50;
    double fineStep = (fStop - fStart) / fineSteps;
    
    maxP = -1.0;
    std::vector<std::pair<double, double>> curve;
    for (int i=0; i<=fineSteps; ++i) {
        double f = fStart + i * fineStep;
        conn.setIsgFreq(f);
        conn.setQsdVfo(2, f);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        double p = measureRms(device, 15);
        curve.push_back({f, p});
        if (p > maxP) { maxP = p; peakF = f; }
    }

    double low3dB = curve.front().first, high3dB = curve.back().first;
    double target = maxP / 1.414;
    bool foundLow = false, foundHigh = false;
    for (size_t i=0; i<curve.size()-1; ++i) {
        if (!foundLow && curve[i].second < target && curve[i+1].second >= target) { low3dB = curve[i].first; foundLow = true; }
        if (curve[i].second >= target && curve[i+1].second < target) { high3dB = curve[i].first; foundHigh = true; }
    }
    if (!foundLow || !foundHigh) { low3dB = fStart; high3dB = fStop; }
    return {peakF, low3dB, high3dB, maxP, high3dB - low3dB};
}

TestStatus presel_cal(RemoteDevice& device, std::string& message) {
    auto& conn = device.conn();
    std::cout << "\n[Presel] Characterizing filter states..." << std::endl;
    std::cout << "Mask | L1 | Peak Freq | -3dB BW   | Q     | Power" << std::endl;
    std::cout << "-----+----+-----------+-----------+-------+-------" << std::endl;

    conn.setAtten(3, true); conn.setAtten(6, true); conn.setAtten(12, true); conn.setAtten(24, true);
    conn.setPgaGain(20.0);
    conn.setIsgEnable(true);
    conn.startStream();
    conn.startReceiving();
    
    struct TestCase { uint32_t mask; bool l1; };
    std::vector<TestCase> cases = {
        { 2047, true  }, // Max C, L1 (LF)
        { 512,  true  }, // Mid C, L1
        { 0,    true  }, // Min C, L1 (HF Edge)
        { 512,  false }, // Mid C, Bypass (VHF Start)
        { 64,   false }, // Small C, Bypass (VHF)
        { 8,    false }, // Tiny C, Bypass (VHF)
        { 0,    false }  // Parasitic floor (Max VHF)
    };

    bool success = true;
    double minFoundPower = 1e9;
    for (const auto& tc : cases) {
        auto resp = characterizeMask(device, tc.mask, tc.l1);
        double q = resp.peakFreq / std::max(1.0, resp.bw3dB);
        std::cout << std::setw(4) << tc.mask << " | " << (tc.l1 ? " Y " : " N ") << " | "
                  << std::fixed << std::setprecision(3) << std::setw(8) << resp.peakFreq / 1e6 << "M | "
                  << std::setw(7) << resp.bw3dB / 1e3 << "k | "
                  << std::setw(5) << std::setprecision(1) << q << " | "
                  << std::setprecision(1) << resp.peakPower << std::endl;
        if (resp.peakPower < 20.0 && (tc.l1 || tc.mask > 0)) { success = false; message = "Weak resonance for mask " + std::to_string(tc.mask); }
        if (resp.peakPower < minFoundPower) minFoundPower = resp.peakPower;
    }

    std::cout << "[Presel] Checking SRF / Out-of-band leaks..." << std::endl;
    conn.setPreselectorInd(0, true);
    for (int i=0; i<11; ++i) conn.setPreselectorCap(i, true);
    double freqs[] = { 28.0e6, 45.0e6 };
    for (double f : freqs) {
        conn.setIsgFreq(f);
        conn.setQsdVfo(2, f);
        double p = measureRms(device, 50);
        std::cout << "Leakage at " << (f/1e6) << "MHz: " << std::fixed << std::setprecision(1) << p << (f > 40e6 ? " (Potential SRF peak)" : "") << std::endl;
    }
    conn.setIsgEnable(false);
    conn.stopStream();
    if (success) { message = "Preselector characterized successfully (Min Power: " + std::to_string(minFoundPower) + ")"; return TestStatus::Passed; }
    return TestStatus::Failed;
}

TestStatus full_scan(RemoteDevice& device, std::string& message) {
    auto& conn = device.conn();
    std::string filename = "preselector-atlas.csv";
    std::ofstream fs(filename);
    if (!fs.is_open()) { message = "Failed to open " + filename; return TestStatus::Failed; }
    fs << "Mask,L1,PeakFreqHz,BW3dB,Q,Power" << std::endl;
    std::cout << "\n[Presel] Generating full atlas to " << filename << " (4096 rows)..." << std::endl;
    std::cout << "Mask | L1 | Peak Freq | -3dB BW   | Q     | Power" << std::endl;
    std::cout << "-----+----+-----------+-----------+-------+-------" << std::endl;
    conn.setAtten(3, true); conn.setAtten(6, true); conn.setAtten(12, true); conn.setAtten(24, true);
    conn.setPgaGain(20.0);
    conn.setIsgEnable(true);
    conn.startStream();
    conn.startReceiving();
    auto lastReport = std::chrono::steady_clock::now();
    for (int l1 = 1; l1 >= 0; --l1) {
        for (uint32_t mask = 0; mask < 2048; ++mask) {
            auto resp = characterizeMask(device, mask, l1 != 0);
            double q = resp.peakFreq / std::max(1.0, resp.bw3dB);
            fs << mask << "," << (l1 ? "Y" : "N") << "," << (long)resp.peakFreq << "," << (long)resp.bw3dB << "," << std::fixed << std::setprecision(2) << q << "," << resp.peakPower << std::endl;
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - lastReport).count() >= 5) {
                std::cout << std::setw(4) << mask << " | " << (l1 ? " Y " : " N ") << " | " << std::fixed << std::setprecision(3) << std::setw(8) << resp.peakFreq / 1e6 << "M | " << std::setw(7) << resp.bw3dB / 1e3 << "k | " << std::setw(5) << std::setprecision(1) << q << " | " << std::setprecision(1) << resp.peakPower << std::endl;
                lastReport = now;
            }
        }
    }
    conn.setIsgEnable(false);
    conn.stopStream();
    message = "Atlas generated to " + filename;
    return TestStatus::Passed;
}

} // namespace nexrx
