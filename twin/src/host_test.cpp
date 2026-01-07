// NexRx Digital Twin - Host Application Test
//
// Standalone test of host-side DSP and visualization.
// Can run with synthetic data or connect to running twin.
//
// Usage:
//   ./host_test                    - Run with synthetic test signal
//   ./host_test --host IP          - Connect to twin via network
//   ./host_test --port CTRL STREAM - Specify ports (default: 5000 5001)
//
// Copyright 2026 NexRx Project - MIT License

#include "host/HostApp.hpp"
#include "host/DspPipeline.hpp"
#include "host/Visualizer.hpp"
#include "stimulus/ToneGenerator.hpp"
#include "transport/IQFrame.hpp"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <thread>

using namespace nexrx;

// Generate synthetic I/Q frame from tone generator
IQFrame generateTestFrame(const ToneGenerator& rf, const ToneGenerator& lo,
                          double time_s, uint32_t sequence) {
    IQFrame frame;
    frame.timestamp_ns = static_cast<uint64_t>(time_s * 1e9);
    frame.sequence = sequence;
    frame.flags = 0;

    // Sample RF signal
    double rf_v = rf.getSample(time_s);

    // Mix with LO (simplified QSD model)
    // Real QSD samples at 4 phases: 0°, 90°, 180°, 270°
    double lo_0 = lo.getSample(time_s);
    double lo_90 = lo.getSample(time_s + 0.25 / 14.0e6);  // 90° delay

    // I = RF * cos(LO), Q = RF * sin(LO)
    // Simplified: multiply RF by LO quadrature
    double i_val = rf_v * lo_0;
    double q_val = rf_v * lo_90;

    // Low-pass filter effect (simplified - just scale down)
    // In real QSD, the sampling capacitor integrates over the switch-on time
    i_val *= 0.5;
    q_val *= 0.5;

    // Add DC bias (1.65V) and convert to ADC
    auto toAdc = [](double v) -> int32_t {
        double biased = v + 1.65;
        double scaled = (biased - 1.65) / 1.65 * 8388607.0;
        return static_cast<int32_t>(std::clamp(scaled, -8388608.0, 8388607.0));
    };

    // All three QSDs get same signal for this test
    for (int ch = 0; ch < 3; ++ch) {
        frame.qsd[ch].i = toAdc(i_val);
        frame.qsd[ch].q = toAdc(q_val);
    }

    return frame;
}

void runSyntheticTest() {
    std::cout << "=== NexRx Host Test - Synthetic Signal ===" << std::endl;
    std::cout << "RF: 14.010 MHz, 1mV peak" << std::endl;
    std::cout << "LO: 14.000 MHz" << std::endl;
    std::cout << "Expected baseband: 10 kHz" << std::endl;
    std::cout << std::endl;

    // Create RF and LO tone generators
    ToneGenerator rf(14.010e6, 0.001);  // 1mV signal
    ToneGenerator lo(14.000e6, 1.0);    // LO at unity

    // Create DSP pipeline
    DspPipeline dsp;
    DspConfig dspConfig;
    dspConfig.enableAgc = false;
    dsp.configure(dspConfig);

    // Create visualizer
    Visualizer viz;

    // Generate and process samples
    constexpr double sampleRate = 96000.0;
    constexpr double samplePeriod = 1.0 / sampleRate;
    constexpr int numSamples = 9600;  // 100ms of data

    std::vector<Complex> outputs;
    outputs.reserve(numSamples);

    std::cout << "Processing " << numSamples << " samples..." << std::endl;

    for (int i = 0; i < numSamples; ++i) {
        double t = i * samplePeriod;
        IQFrame frame = generateTestFrame(rf, lo, t, i);

        Complex output = dsp.process(frame);
        outputs.push_back(output);
        viz.update(output);
    }

    // Display final visualization
    std::cout << std::endl;
    std::cout << viz.render() << std::endl;

    // Analyze baseband frequency
    std::cout << "=== Signal Analysis ===" << std::endl;

    // Count zero crossings to estimate frequency
    int crossings = 0;
    for (size_t i = 1; i < outputs.size(); ++i) {
        if ((outputs[i-1].real() >= 0 && outputs[i].real() < 0) ||
            (outputs[i-1].real() < 0 && outputs[i].real() >= 0)) {
            ++crossings;
        }
    }

    double estFreq = (crossings / 2.0) * sampleRate / outputs.size();
    std::cout << "Estimated baseband freq: " << estFreq / 1000.0 << " kHz" << std::endl;

    // RMS level
    double sumSq = 0;
    for (const auto& s : outputs) {
        sumSq += std::norm(s);
    }
    double rms = std::sqrt(sumSq / outputs.size());
    std::cout << "RMS level: " << rms * 1000.0 << " mV" << std::endl;
    std::cout << "Level: " << viz.levelDbm() << " dBm" << std::endl;
    std::cout << "S-meter: S" << viz.sUnits() << std::endl;
}

void runNetworkTest(const std::string& host, uint16_t controlPort, uint16_t streamPort) {
    std::cout << "=== NexRx Host Test - Network ===" << std::endl;
    std::cout << "Connecting to: " << host << " (TCP:" << controlPort << ", UDP:" << streamPort << ")" << std::endl;

    HostApp hostApp;
    HostConfig config;
    config.host = host;
    config.controlPort = controlPort;
    config.streamPort = streamPort;
    config.verbose = true;

    if (!hostApp.initialize(config)) {
        std::cerr << "Failed to connect to twin" << std::endl;
        std::cerr << "Make sure the twin is running with --stream" << std::endl;
        return;
    }

    // Create DSP pipeline and visualizer
    DspPipeline dsp;
    DspConfig dspConfig;
    dsp.configure(dspConfig);

    Visualizer viz;

    // Set up frame callback
    hostApp.setFrameCallback([&](const IQFrame& frame) {
        Complex output = dsp.process(frame);
        viz.update(output);
    });

    std::cout << "Receiving... Press Ctrl+C to stop" << std::endl;
    std::cout << std::endl;

    // Poll and display
    auto lastDisplay = std::chrono::steady_clock::now();

    while (true) {
        hostApp.pollFrames(100);

        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration<double>(now - lastDisplay).count() > 0.1) {
            viz.display();
            lastDisplay = now;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void printUsage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --host IP     Connect to twin at IP (default: 127.0.0.1)" << std::endl;
    std::cout << "  --port C S    TCP control port C, UDP stream port S (default: 5000 5001)" << std::endl;
    std::cout << "  --help        Show this help" << std::endl;
    std::cout << std::endl;
    std::cout << "Without --host, runs with synthetic test signal." << std::endl;
}

int main(int argc, char* argv[]) {
    // Parse arguments
    std::string host;
    uint16_t controlPort = 5000;
    uint16_t streamPort = 5001;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            printUsage(argv[0]);
            return 0;
        } else if (std::strcmp(argv[i], "--host") == 0) {
            if (i + 1 < argc) {
                host = argv[++i];
            } else {
                std::cerr << "Error: --host requires an IP address" << std::endl;
                return 1;
            }
        } else if (std::strcmp(argv[i], "--port") == 0) {
            if (i + 2 < argc) {
                controlPort = static_cast<uint16_t>(std::atoi(argv[++i]));
                streamPort = static_cast<uint16_t>(std::atoi(argv[++i]));
            } else {
                std::cerr << "Error: --port requires two port numbers" << std::endl;
                return 1;
            }
        } else {
            std::cerr << "Unknown option: " << argv[i] << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }

    if (host.empty()) {
        runSyntheticTest();
    } else {
        runNetworkTest(host, controlPort, streamPort);
    }

    return 0;
}
