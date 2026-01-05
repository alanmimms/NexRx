// NexRx Digital Twin - Pipeline Test
//
// End-to-end test of the RF simulation pipeline:
// Xyce simulation → ADC Sampler → Shared Memory → Console output
//
// Copyright 2026 NexRx Project - MIT License

#include "orchestrator/Orchestrator.hpp"
#include "sampler/AdcSampler.hpp"
#include "sampler/RxControls.hpp"
#include "transport/IQFrame.hpp"
#include "transport/SharedMemTransport.hpp"

#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <chrono>

using namespace NexRx::Twin;
using namespace nexrx;

// Simple console output of I/Q samples
void printFrame(const IQFrame& frame) {
    // Convert to voltage for display
    constexpr double scale = 1.65 / 8388607.0;

    double q0_i = frame.qsd[0].i * scale;
    double q0_q = frame.qsd[0].q * scale;
    double q1_i = frame.qsd[1].i * scale;
    double q1_q = frame.qsd[1].q * scale;
    double q2_i = frame.qsd[2].i * scale;
    double q2_q = frame.qsd[2].q * scale;

    std::cout << std::fixed << std::setprecision(6);
    std::cout << "t=" << std::setw(10) << frame.timestamp_ns / 1000.0 << "us"
              << " Q0: " << std::setw(8) << q0_i * 1000 << "/" << std::setw(8) << q0_q * 1000 << "mV"
              << " Q1: " << std::setw(8) << q1_i * 1000 << "/" << std::setw(8) << q1_q * 1000 << "mV"
              << " Q2: " << std::setw(8) << q2_i * 1000 << "/" << std::setw(8) << q2_q * 1000 << "mV"
              << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "=== NexRx Digital Twin Pipeline Test ===" << std::endl;

    // Default netlist path
    std::string netlistPath = "netlists/pipeline_test.cir";
    if (argc > 1) {
        netlistPath = argv[1];
    }

    // Duration to simulate (default 1ms = 96 samples)
    double duration_s = 0.001;
    if (argc > 2) {
        duration_s = std::atof(argv[2]);
    }

    std::cout << "Netlist: " << netlistPath << std::endl;
    std::cout << "Duration: " << duration_s * 1000 << " ms" << std::endl;

    // Create orchestrator
    Orchestrator orchestrator;

    OrchestratorConfig config;
    config.netlistPath = netlistPath;
    config.simulationTimeStep_ns = 5.0;  // 5ns steps for 14MHz RF
    config.adcSampleRate_Hz = 96000.0;
    config.realTimeMode = false;
    config.verbose = true;

    std::cout << "\n--- Initializing Xyce ---" << std::endl;
    if (!orchestrator.initialize(config)) {
        std::cerr << "Failed to initialize orchestrator" << std::endl;
        return 1;
    }

    // Configure ADC sampler with node names from pipeline_test.cir
    AdcConfig adcConfig;
    adcConfig.i_nodes = {"Q0_I", "Q1_I", "Q2_I"};
    adcConfig.q_nodes = {"Q0_Q", "Q1_Q", "Q2_Q"};

    // Access the Xyce wrapper through node voltage getter
    // Note: AdcSampler needs direct XyceWrapper access, but for this test
    // we'll use the callback mechanism

    // Storage for samples
    std::vector<IQFrame> frames;
    frames.reserve(static_cast<size_t>(duration_s * 96000 + 100));

    uint64_t sampleCount = 0;
    double nextSampleTime_s = 1.0 / 96000.0;
    constexpr double samplePeriod_s = 1.0 / 96000.0;

    // Set up ADC callback to sample node voltages
    orchestrator.setAdcSampleCallback([&](double time_s, uint64_t index) {
        IQFrame frame;
        frame.timestamp_ns = static_cast<uint64_t>(time_s * 1e9);
        frame.sequence = static_cast<uint32_t>(index);
        frame.flags = 0;

        // Sample each QSD
        for (int ch = 0; ch < 3; ++ch) {
            std::string i_node = adcConfig.i_nodes[ch];
            std::string q_node = adcConfig.q_nodes[ch];

            auto v_i = orchestrator.getNodeVoltage(i_node);
            auto v_q = orchestrator.getNodeVoltage(q_node);

            if (v_i && v_q) {
                // Convert voltage to ADC value
                // Remove 1.65V DC bias, scale to 24-bit
                double vi = *v_i - 1.65;
                double vq = *v_q - 1.65;

                constexpr double scale = 8388607.0 / 1.65;
                frame.qsd[ch].i = static_cast<int32_t>(std::clamp(vi * scale, -8388608.0, 8388607.0));
                frame.qsd[ch].q = static_cast<int32_t>(std::clamp(vq * scale, -8388608.0, 8388607.0));
            } else {
                frame.qsd[ch] = IQSample{0, 0};
                frame.flags |= 0x01;  // Mark error
            }
        }

        frames.push_back(frame);
        sampleCount++;
    });

    std::cout << "\n--- Running Simulation ---" << std::endl;
    auto startTime = std::chrono::steady_clock::now();

    bool success = orchestrator.runFor(duration_s);

    auto endTime = std::chrono::steady_clock::now();
    double elapsedSec = std::chrono::duration<double>(endTime - startTime).count();

    std::cout << "\n--- Results ---" << std::endl;
    std::cout << "Success: " << (success ? "yes" : "no") << std::endl;
    std::cout << "Samples collected: " << frames.size() << std::endl;
    std::cout << "Wall time: " << elapsedSec << " s" << std::endl;
    std::cout << "Speed: " << (duration_s / elapsedSec) << "x realtime" << std::endl;

    // Print first 10 and last 10 samples
    std::cout << "\n--- First 10 samples ---" << std::endl;
    for (size_t i = 0; i < std::min(frames.size(), size_t(10)); ++i) {
        printFrame(frames[i]);
    }

    if (frames.size() > 20) {
        std::cout << "\n... (" << frames.size() - 20 << " samples omitted) ..." << std::endl;
    }

    if (frames.size() > 10) {
        std::cout << "\n--- Last 10 samples ---" << std::endl;
        for (size_t i = frames.size() - 10; i < frames.size(); ++i) {
            printFrame(frames[i]);
        }
    }

    // Analyze I/Q amplitude
    if (!frames.empty()) {
        double sumMagSq = 0;
        for (const auto& f : frames) {
            sumMagSq += f.qsd[0].magnitudeSquared();
        }
        double rmsMag = std::sqrt(sumMagSq / frames.size());
        double rmsVoltage = rmsMag * (1.65 / 8388607.0);

        std::cout << "\n--- Signal Analysis (QSD0) ---" << std::endl;
        std::cout << "RMS magnitude: " << rmsVoltage * 1000 << " mV" << std::endl;

        // Expected: 10kHz baseband from 14.01MHz - 14.00MHz
        std::cout << "Expected baseband: 10 kHz" << std::endl;
    }

    return success ? 0 : 1;
}
