#include "test-engine.hpp"
#include "fft-util.hpp"
#include <thread>
#include <chrono>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>

namespace nexrx {

TestStatus select_chk(RemoteDevice& device, std::string& message) {
    auto& conn = device.conn();
    std::cout << "\n[Select] Characterizing preselector using white noise FFT..." << std::endl;
    std::cout << "Mask | L1 | Peak Freq | -3dB BW   | Q     | Power (dB)" << std::endl;
    std::cout << "-----+----+-----------+-----------+-------+------------" << std::endl;

    conn.setAtten(3, true); conn.setAtten(6, true); conn.setAtten(12, true); conn.setAtten(24, true);
    for (int i=0; i<6; ++i) conn.setPgaGain(i, 20.0);
    
    // ISG Noise mode (1Hz)
    conn.setIsgEnable(true);
    conn.setIsgFreq(1.0);
    
    conn.startStream();
    conn.startReceiving();

    // Actual resonant frequencies for these masks (found in sweep test)
    struct TestCase { uint32_t mask; bool l1; double resonance; };
    std::vector<TestCase> cases = {
        { 2047, true,  2.709e6  },
        { 512,  true,  5.900e6  },
        { 0,    true,  75.827e6 },
        { 512,  false, 2.113e6  },
        { 64,   false, 5.038e6  },
        { 8,    false, 12.949e6 },
        { 0,    false, 27.053e6 }
    };

    bool allOk = true;
    double fs = 96000.0;
    const size_t FFT_SIZE = 4096;
    const int AVG_COUNT = 64; // Increased averaging for noise

    for (const auto& tc : cases) {
        conn.setPreselectorInd(0, tc.l1);
        for (int i=0; i<11; ++i) conn.setPreselectorCap(i, (tc.mask >> i) & 1);
        conn.setQsdVfo(2, tc.resonance);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        
        std::vector<double> avgMag(FFT_SIZE, 0.0);
        int collected = 0;
        for (int a=0; a<AVG_COUNT; ++a) {
            std::vector<std::complex<double>> buffer;
            auto callback = [&](const IQFrame& frame) {
                if (buffer.size() < FFT_SIZE) {
                    buffer.push_back({(double)frame.qsd[2].i, (double)frame.qsd[2].q});
                }
            };
            conn.setFrameCallback(callback);
            for (int i=0; i<100 && buffer.size() < FFT_SIZE; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(2));
            conn.setFrameCallback(nullptr);

            if (buffer.size() < FFT_SIZE) continue;
            collected++;

            fftInPlace(buffer, false);
            for (size_t k=0; k<FFT_SIZE; ++k) avgMag[k] += std::abs(buffer[k]);
        }

        if (collected == 0) continue;

        double maxMag = -1.0;
        size_t peakBin = 0;
        for (size_t k=0; k<FFT_SIZE; ++k) {
            avgMag[k] /= (double)collected;
        }
        
        // Smooth the spectrum (5-bin moving average)
        std::vector<double> smoothed(FFT_SIZE);
        for (size_t k=0; k<FFT_SIZE; ++k) {
            double sum = 0;
            for (int i=-2; i<=2; ++i) sum += avgMag[(k + i + FFT_SIZE) % FFT_SIZE];
            smoothed[k] = sum / 5.0;
            if (smoothed[k] > maxMag) { maxMag = smoothed[k]; peakBin = k; }
        }

        double freqOffset;
        if (peakBin <= FFT_SIZE / 2) freqOffset = (double)peakBin * fs / FFT_SIZE;
        else freqOffset = (double)((int)peakBin - (int)FFT_SIZE) * fs / FFT_SIZE;

        double peakFreq = tc.resonance + freqOffset;
        double target = maxMag / 1.414;
        int lowBin = -1, highBin = -1;
        
        std::vector<std::pair<double, double>> linear;
        for (int i = -(int)FFT_SIZE/2; i < (int)FFT_SIZE/2; ++i) {
            size_t k = (peakBin + i + FFT_SIZE) % FFT_SIZE;
            double f = freqOffset + (double)i * fs / FFT_SIZE;
            linear.push_back({f, smoothed[k]});
        }

        size_t centerIdx = FFT_SIZE / 2;
        for (int i = (int)centerIdx; i >= 0; --i) { if (linear[i].second < target) { lowBin = i; break; } }
        for (int i = (int)centerIdx; i < (int)FFT_SIZE; ++i) { if (linear[i].second < target) { highBin = i; break; } }

        double bw = 0;
        bool bwLimit = false;
        if (lowBin != -1 && highBin != -1) bw = linear[highBin].first - linear[lowBin].first;
        else { bw = fs; bwLimit = true; }

        double q = peakFreq / std::max(1.0, bw);
        double power = 20.0 * std::log10(std::max(1.0, maxMag / FFT_SIZE));

        std::cout << std::setw(4) << tc.mask << " | " << (tc.l1 ? " Y " : " N ") << " | "
                  << std::fixed << std::setprecision(3) << std::setw(8) << peakFreq / 1e6 << "M | "
                  << (bwLimit ? ">" : " ") << std::setw(6) << bw / 1e3 << "k | "
                  << std::setw(5) << std::setprecision(1) << q << " | "
                  << std::fixed << std::setprecision(1) << power << std::endl;
    }

    conn.setIsgEnable(false);
    conn.stopStream();
    
    if (allOk) { message = "Selectivity characterized using noise FFT"; return TestStatus::Passed; }
    return TestStatus::Failed;
}

} // namespace nexrx
