// NexRx Digital Twin - Pipeline Test
//
// End-to-end test of the RF simulation pipeline.
// Two modes:
//   --functional : Pure C++ model, runs at real-time or faster
//   (default)    : Full Xyce SPICE simulation, accurate but slow
//
// Copyright 2026 NexRx Project - MIT License

#include "orchestrator/Orchestrator.hpp"
#include "sampler/AdcSampler.hpp"
#include "sampler/RxControls.hpp"
#include "transport/IQFrame.hpp"
#include "transport/SharedMemTransport.hpp"
#include "stimulus/ToneGenerator.hpp"

#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstring>
#include <vector>
#include <chrono>
#include <thread>

using namespace NexRx::Twin;
using namespace nexrx;

//=============================================================================
// Command line options
//=============================================================================
struct Options {
    bool functional = false;      // Use fast C++ model instead of Xyce
    bool help = false;
    bool verbose = true;
    double duration_ms = 1.0;     // Simulation duration in ms
    double rf_freq_mhz = 14.010;  // RF signal frequency
    double lo_freq_mhz = 14.000;  // LO frequency
    double rf_amplitude_mv = 1.0; // RF amplitude in mV
    std::string netlist = "netlists/pipeline_test.cir";
};

void printUsage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n"
              << "\n"
              << "Modes:\n"
              << "  --functional    Fast C++ functional model (real-time capable)\n"
              << "  (default)       Full Xyce SPICE physics simulation (slow but accurate)\n"
              << "\n"
              << "Options:\n"
              << "  --duration MS   Simulation duration in milliseconds (default: 1.0)\n"
              << "  --netlist FILE  Xyce netlist path (physics mode only)\n"
              << "  --rf FREQ       RF frequency in MHz (default: 14.010)\n"
              << "  --lo FREQ       LO frequency in MHz (default: 14.000)\n"
              << "  --amplitude MV  RF amplitude in mV (default: 1.0)\n"
              << "  --quiet         Suppress progress output\n"
              << "  --help          Show this help\n"
              << "\n"
              << "Examples:\n"
              << "  " << prog << " --functional --duration 100\n"
              << "      Run 100ms functional simulation (fast)\n"
              << "\n"
              << "  " << prog << " --duration 0.1 --netlist netlists/qsd.cir\n"
              << "      Run 0.1ms Xyce simulation with custom netlist\n"
              << std::endl;
}

Options parseArgs(int argc, char* argv[]) {
    Options opts;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            opts.help = true;
        } else if (strcmp(argv[i], "--functional") == 0 || strcmp(argv[i], "-f") == 0) {
            opts.functional = true;
        } else if (strcmp(argv[i], "--quiet") == 0 || strcmp(argv[i], "-q") == 0) {
            opts.verbose = false;
        } else if (strcmp(argv[i], "--duration") == 0 && i + 1 < argc) {
            opts.duration_ms = std::atof(argv[++i]);
        } else if (strcmp(argv[i], "--netlist") == 0 && i + 1 < argc) {
            opts.netlist = argv[++i];
        } else if (strcmp(argv[i], "--rf") == 0 && i + 1 < argc) {
            opts.rf_freq_mhz = std::atof(argv[++i]);
        } else if (strcmp(argv[i], "--lo") == 0 && i + 1 < argc) {
            opts.lo_freq_mhz = std::atof(argv[++i]);
        } else if (strcmp(argv[i], "--amplitude") == 0 && i + 1 < argc) {
            opts.rf_amplitude_mv = std::atof(argv[++i]);
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            opts.help = true;
        }
    }

    return opts;
}

//=============================================================================
// Frame output
//=============================================================================
void printFrame(const IQFrame& frame) {
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

void analyzeFrames(const std::vector<IQFrame>& frames) {
    if (frames.empty()) return;

    // Print first 10 samples
    std::cout << "\n--- First 10 samples ---" << std::endl;
    for (size_t i = 0; i < std::min(frames.size(), size_t(10)); ++i) {
        printFrame(frames[i]);
    }

    if (frames.size() > 20) {
        std::cout << "\n... (" << frames.size() - 20 << " samples omitted) ..." << std::endl;
    }

    // Print last 10 samples
    if (frames.size() > 10) {
        std::cout << "\n--- Last 10 samples ---" << std::endl;
        for (size_t i = frames.size() - 10; i < frames.size(); ++i) {
            printFrame(frames[i]);
        }
    }

    // RMS analysis
    double sumMagSq = 0;
    for (const auto& f : frames) {
        sumMagSq += f.qsd[0].magnitudeSquared();
    }
    double rmsMag = std::sqrt(sumMagSq / frames.size());
    double rmsVoltage = rmsMag * (1.65 / 8388607.0);

    std::cout << "\n--- Signal Analysis (QSD0) ---" << std::endl;
    std::cout << "RMS magnitude: " << rmsVoltage * 1000 << " mV" << std::endl;

    // Estimate baseband frequency via zero-crossings
    int crossings = 0;
    for (size_t i = 1; i < frames.size(); ++i) {
        if ((frames[i-1].qsd[0].i >= 0 && frames[i].qsd[0].i < 0) ||
            (frames[i-1].qsd[0].i < 0 && frames[i].qsd[0].i >= 0)) {
            ++crossings;
        }
    }
    double estFreq = (crossings / 2.0) * 96000.0 / frames.size();
    std::cout << "Estimated baseband: " << estFreq / 1000.0 << " kHz" << std::endl;
}

//=============================================================================
// Functional mode - Pure C++ QSD model (FAST)
//=============================================================================
int runFunctionalMode(const Options& opts) {
    std::cout << "=== NexRx Pipeline Test - FUNCTIONAL MODE ===" << std::endl;
    std::cout << "RF: " << opts.rf_freq_mhz << " MHz, " << opts.rf_amplitude_mv << " mV" << std::endl;
    std::cout << "LO: " << opts.lo_freq_mhz << " MHz" << std::endl;
    std::cout << "Baseband: " << std::abs(opts.rf_freq_mhz - opts.lo_freq_mhz) * 1000 << " kHz" << std::endl;
    std::cout << "Duration: " << opts.duration_ms << " ms" << std::endl;
    std::cout << std::endl;

    const double rf_freq = opts.rf_freq_mhz * 1e6;
    const double lo_freq = opts.lo_freq_mhz * 1e6;
    const double rf_amp = opts.rf_amplitude_mv * 1e-3;  // Convert to volts

    constexpr double sampleRate = 96000.0;
    constexpr double samplePeriod = 1.0 / sampleRate;
    const double duration_s = opts.duration_ms / 1000.0;
    const size_t numSamples = static_cast<size_t>(duration_s * sampleRate);

    std::vector<IQFrame> frames;
    frames.reserve(numSamples);

    auto startTime = std::chrono::steady_clock::now();

    // QSD model parameters
    // Real QSD integrates over switch-on time, here we model as ideal mixer + LPF
    constexpr double lpfAlpha = 0.1;  // Simple IIR LPF coefficient
    double lpf_i[3] = {0, 0, 0};
    double lpf_q[3] = {0, 0, 0};

    for (size_t i = 0; i < numSamples; ++i) {
        double t = i * samplePeriod;

        // RF signal (could be extended to use ToneGenerator for complex signals)
        double rf = rf_amp * std::sin(2.0 * M_PI * rf_freq * t);

        // Add some noise for realism
        double noise = (rand() / (double)RAND_MAX - 0.5) * 1e-6;  // 1uV noise
        rf += noise;

        IQFrame frame;
        frame.timestamp_ns = static_cast<uint64_t>(t * 1e9);
        frame.sequence = static_cast<uint32_t>(i);
        frame.flags = 0;

        // Model each QSD channel
        for (int ch = 0; ch < 3; ++ch) {
            // Phase offset per channel (could model different LO phases)
            double phase_offset = ch * 0.0;  // All same for now

            // Ideal quadrature mixer: multiply by LO
            double lo_i = std::cos(2.0 * M_PI * lo_freq * t + phase_offset);
            double lo_q = std::sin(2.0 * M_PI * lo_freq * t + phase_offset);

            double mixed_i = rf * lo_i;
            double mixed_q = rf * lo_q;

            // Simple LPF to remove 2*LO component (real QSD does this via integration)
            lpf_i[ch] = lpf_i[ch] * (1.0 - lpfAlpha) + mixed_i * lpfAlpha;
            lpf_q[ch] = lpf_q[ch] * (1.0 - lpfAlpha) + mixed_q * lpfAlpha;

            // Scale to ADC range (24-bit, +/-1.65V full scale)
            // The mixer output is roughly rf_amp/2 after LPF
            constexpr double adcScale = 8388607.0 / 1.65;
            frame.qsd[ch].i = static_cast<int32_t>(std::clamp(lpf_i[ch] * adcScale, -8388608.0, 8388607.0));
            frame.qsd[ch].q = static_cast<int32_t>(std::clamp(lpf_q[ch] * adcScale, -8388608.0, 8388607.0));
        }

        frames.push_back(frame);

        // Progress reporting
        if (opts.verbose && i % 9600 == 0 && i > 0) {
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - startTime).count();
            double progress = 100.0 * i / numSamples;
            double simTime = i * samplePeriod;
            double speed = simTime / elapsed;

            std::cout << "\r[Functional] " << std::fixed << std::setprecision(1)
                      << progress << "% | "
                      << simTime * 1000 << " ms | "
                      << i << " samples | "
                      << speed << "x realtime     " << std::flush;
        }
    }

    auto endTime = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(endTime - startTime).count();

    if (opts.verbose) {
        std::cout << "\r" << std::string(70, ' ') << "\r";
    }

    std::cout << "\n--- Results ---" << std::endl;
    std::cout << "Samples: " << frames.size() << std::endl;
    std::cout << "Wall time: " << elapsed * 1000 << " ms" << std::endl;
    std::cout << "Speed: " << (opts.duration_ms / 1000.0) / elapsed << "x realtime" << std::endl;

    analyzeFrames(frames);

    return 0;
}

//=============================================================================
// Physics mode - Full Xyce SPICE simulation (ACCURATE)
//=============================================================================
int runPhysicsMode(const Options& opts) {
    std::cout << "=== NexRx Pipeline Test - PHYSICS MODE ===" << std::endl;
    std::cout << "Netlist: " << opts.netlist << std::endl;
    std::cout << "Duration: " << opts.duration_ms << " ms" << std::endl;
    std::cout << "(This will be slow - use --functional for fast testing)" << std::endl;
    std::cout << std::endl;

    double duration_s = opts.duration_ms / 1000.0;

    Orchestrator orchestrator;

    OrchestratorConfig config;
    config.netlistPath = opts.netlist;
    config.simulationTimeStep_ns = 5.0;  // 5ns steps for 14MHz RF
    config.adcSampleRate_Hz = 96000.0;
    config.realTimeMode = false;
    config.verbose = opts.verbose;

    std::cout << "--- Initializing Xyce ---" << std::endl;
    if (!orchestrator.initialize(config)) {
        std::cerr << "Failed to initialize orchestrator" << std::endl;
        std::cerr << "Check that netlist exists: " << opts.netlist << std::endl;
        return 1;
    }

    // Configure ADC sampler with node names
    AdcConfig adcConfig;
    adcConfig.i_nodes = {"Q0_I", "Q1_I", "Q2_I"};
    adcConfig.q_nodes = {"Q0_Q", "Q1_Q", "Q2_Q"};

    std::vector<IQFrame> frames;
    frames.reserve(static_cast<size_t>(duration_s * 96000 + 100));

    // Set up ADC callback
    orchestrator.setAdcSampleCallback([&](double time_s, uint64_t index) {
        IQFrame frame;
        frame.timestamp_ns = static_cast<uint64_t>(time_s * 1e9);
        frame.sequence = static_cast<uint32_t>(index);
        frame.flags = 0;

        for (int ch = 0; ch < 3; ++ch) {
            auto v_i = orchestrator.getNodeVoltage(adcConfig.i_nodes[ch]);
            auto v_q = orchestrator.getNodeVoltage(adcConfig.q_nodes[ch]);

            if (v_i && v_q) {
                double vi = *v_i - 1.65;
                double vq = *v_q - 1.65;
                constexpr double scale = 8388607.0 / 1.65;
                frame.qsd[ch].i = static_cast<int32_t>(std::clamp(vi * scale, -8388608.0, 8388607.0));
                frame.qsd[ch].q = static_cast<int32_t>(std::clamp(vq * scale, -8388608.0, 8388607.0));
            } else {
                frame.qsd[ch] = IQSample{0, 0};
                frame.flags |= 0x01;
            }
        }

        frames.push_back(frame);
    });

    std::cout << "\n--- Running Xyce Simulation ---" << std::endl;
    auto startTime = std::chrono::steady_clock::now();

    bool success = orchestrator.runFor(duration_s);

    auto endTime = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(endTime - startTime).count();

    std::cout << "\n--- Results ---" << std::endl;
    std::cout << "Success: " << (success ? "yes" : "no") << std::endl;
    std::cout << "Samples: " << frames.size() << std::endl;
    std::cout << "Wall time: " << elapsed << " s" << std::endl;
    std::cout << "Speed: " << (duration_s / elapsed) << "x realtime" << std::endl;

    analyzeFrames(frames);

    return success ? 0 : 1;
}

//=============================================================================
// Main
//=============================================================================
int main(int argc, char* argv[]) {
    Options opts = parseArgs(argc, argv);

    if (opts.help) {
        printUsage(argv[0]);
        return 0;
    }

    if (opts.functional) {
        return runFunctionalMode(opts);
    } else {
        return runPhysicsMode(opts);
    }
}
