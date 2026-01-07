// NexRx Digital Twin - Signal Generator
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
#include "stimulus/StimulusLua.hpp"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <cmath>
#include <cstring>
#include <vector>
#include <chrono>
#include <thread>
#include <memory>

using namespace NexRx::Twin;
using namespace nexrx;

//=============================================================================
// Command line options
//=============================================================================
struct Options {
    bool functional = false;      // Use fast C++ model instead of Xyce
    bool help = false;
    bool verbose = true;
    bool stream = false;          // Stream to shared memory for app display
    double duration_ms = 0.0;     // Simulation duration in ms (0 = run forever)
    double rf_freq_mhz = 14.201;  // RF signal frequency (1kHz above LO for USB)
    double lo_freq_mhz = 14.200;  // LO frequency (matches app default VFO)
    double rf_amplitude_mv = 1.0; // RF amplitude in mV
    std::string netlist = "netlists/pipeline_test.cir";
    std::string stimulus = "";    // Stimulus script (empty = use simple tone)
};

void printUsage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n"
              << "\n"
              << "Modes:\n"
              << "  --functional    Fast C++ functional model (real-time capable)\n"
              << "  (default)       Full Xyce SPICE physics simulation (slow but accurate)\n"
              << "\n"
              << "Options:\n"
              << "  --stream        Stream I/Q to shared memory for app display\n"
              << "  --stimulus FILE Lua stimulus script (default: config/stimuli/default.lua)\n"
              << "  --duration MS   Simulation duration in milliseconds (0 = forever, default)\n"
              << "  --netlist FILE  Xyce netlist path (physics mode only)\n"
              << "  --rf FREQ       RF frequency in MHz (for simple tone, no stimulus script)\n"
              << "  --lo FREQ       LO frequency in MHz (default: 14.200)\n"
              << "  --amplitude MV  RF amplitude in mV (for simple tone, default: 1.0)\n"
              << "  --quiet         Suppress progress output\n"
              << "  --help          Show this help\n"
              << "\n"
              << "Examples:\n"
              << "  " << prog << " --functional --stream\n"
              << "      Stream with default stimuli (CW beacons, SSB signals, etc.)\n"
              << "\n"
              << "  " << prog << " --functional --stream --rf 14.025 --lo 14.024\n"
              << "      Stream simple 1kHz tone (no stimulus script)\n"
              << "\n"
              << "  " << prog << " --functional --stimulus config/stimuli/contest.lua\n"
              << "      Use custom stimulus configuration\n"
              << std::endl;
}

Options parseArgs(int argc, char* argv[]) {
    Options opts;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            opts.help = true;
        } else if (strcmp(argv[i], "--functional") == 0 || strcmp(argv[i], "-f") == 0) {
            opts.functional = true;
        } else if (strcmp(argv[i], "--stream") == 0 || strcmp(argv[i], "-s") == 0) {
            opts.stream = true;
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
        } else if (strcmp(argv[i], "--stimulus") == 0 && i + 1 < argc) {
            opts.stimulus = argv[++i];
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
// Control file reader - check for LO frequency updates from app
//=============================================================================
bool readControlFile(double& lo_freq_hz) {
    std::ifstream ctl("/tmp/nexrx_control");
    if (!ctl) return false;

    std::string cmd;
    double freq;
    if (ctl >> cmd >> freq) {
        if (cmd == "LO" && freq > 0) {
            lo_freq_hz = freq;
            return true;
        }
    }
    return false;
}

//=============================================================================
// Functional mode - Pure C++ QSD model (FAST)
//=============================================================================
int runFunctionalMode(const Options& opts) {
    const bool runForever = (opts.duration_ms <= 0);

    std::cout << "=== NexRx Signal Generator - FUNCTIONAL MODE ===" << std::endl;
    std::cout << std::fixed << std::setprecision(6);

    // Determine stimulus source
    std::string stimulusPath = opts.stimulus;
    bool useStimulus = true;

    // Default to config/stimuli/default.lua if not specified and --rf not given
    if (stimulusPath.empty()) {
        // Check if --rf was explicitly set (different from default)
        if (opts.rf_freq_mhz != 14.201) {
            useStimulus = false;  // Use simple tone mode
        } else {
            stimulusPath = "config/stimuli/default.lua";
        }
    }

    // Set up stimulus system
    std::shared_ptr<StimulusManager> stimulusManager;
    std::unique_ptr<sol::state> lua;

    if (useStimulus) {
        // Check if stimulus file exists
        std::ifstream check(stimulusPath);
        if (!check.good()) {
            std::cerr << "Warning: Stimulus file not found: " << stimulusPath << std::endl;
            std::cerr << "Falling back to simple tone mode" << std::endl;
            useStimulus = false;
        } else {
            check.close();

            lua = std::make_unique<sol::state>();
            lua->open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);

            StimulusLua stimLua;
            stimLua.registerBindings(*lua);

            std::cout << "Loading stimulus: " << stimulusPath << std::endl;
            if (!stimLua.loadScript(*lua, stimulusPath)) {
                std::cerr << "Failed to load stimulus script" << std::endl;
                return 1;
            }

            stimulusManager = stimLua.manager();
            std::cout << "Loaded " << stimulusManager->count() << " stimuli" << std::endl;
        }
    }

    if (!useStimulus) {
        std::cout << "RF: " << opts.rf_freq_mhz << " MHz, " << opts.rf_amplitude_mv << " mV" << std::endl;
        std::cout << "Baseband: " << std::abs(opts.rf_freq_mhz - opts.lo_freq_mhz) * 1000 << " kHz" << std::endl;
    }

    std::cout << "LO: " << opts.lo_freq_mhz << " MHz (tunable via /tmp/nexrx_control)" << std::endl;
    if (runForever) {
        std::cout << "Duration: forever (Ctrl+C to stop)" << std::endl;
    } else {
        std::cout << "Duration: " << opts.duration_ms << " ms" << std::endl;
    }
    if (opts.stream) {
        std::cout << "Streaming: /nexrx_iq (run nexrx_app to view)" << std::endl;
    }
    std::cout << std::endl;

    const double rf_freq = opts.rf_freq_mhz * 1e6;
    double lo_freq = opts.lo_freq_mhz * 1e6;  // Mutable - can be updated via control file
    const double rf_amp = opts.rf_amplitude_mv * 1e-3;  // Convert to volts

    constexpr double sampleRate = 96000.0;
    constexpr double samplePeriod = 1.0 / sampleRate;
    const double duration_s = runForever ? 1e9 : opts.duration_ms / 1000.0;  // ~31 years if forever
    const size_t numSamples = runForever ? SIZE_MAX : static_cast<size_t>(duration_s * sampleRate);

    // Set up shared memory transport if streaming
    std::unique_ptr<SharedMemTransport> transport;
    if (opts.stream) {
        SharedMemConfig shmConfig;
        shmConfig.name = "/nexrx_iq";
        shmConfig.capacity = 8192;
        shmConfig.create = true;  // We're the producer

        transport = std::make_unique<SharedMemTransport>(shmConfig);
        if (!transport->connect()) {
            std::cerr << "Failed to create shared memory transport" << std::endl;
            return 1;
        }
        std::cout << "[Stream] Created shared memory: " << shmConfig.name << std::endl;
    }

    std::vector<IQFrame> frames;
    if (!opts.stream) {
        frames.reserve(numSamples);
    }

    auto startTime = std::chrono::steady_clock::now();

    // Anti-alias LPF: 2nd-order Butterworth at 48kHz (Nyquist for 96kHz)
    // Coefficients for fs=96kHz, fc=48kHz (actually use ~40kHz for margin)
    // Using biquad: y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
    // For Butterworth fc=40kHz at fs=96kHz:
    constexpr double lpf_b0 = 0.292893;
    constexpr double lpf_b1 = 0.585786;
    constexpr double lpf_b2 = 0.292893;
    constexpr double lpf_a1 = -0.0;
    constexpr double lpf_a2 = 0.171573;

    // Filter state for each QSD channel (I and Q)
    double lpf_xi[3][2] = {{0,0}, {0,0}, {0,0}};  // x[n-1], x[n-2]
    double lpf_yi[3][2] = {{0,0}, {0,0}, {0,0}};  // y[n-1], y[n-2]
    double lpf_xq[3][2] = {{0,0}, {0,0}, {0,0}};
    double lpf_yq[3][2] = {{0,0}, {0,0}, {0,0}};

    auto applyLpf = [&](double x, double* xi, double* yi) -> double {
        double y = lpf_b0 * x + lpf_b1 * xi[0] + lpf_b2 * xi[1]
                   - lpf_a1 * yi[0] - lpf_a2 * yi[1];
        xi[1] = xi[0]; xi[0] = x;
        yi[1] = yi[0]; yi[0] = y;
        return y;
    };

    for (size_t i = 0; i < numSamples; ++i) {
        double t = i * samplePeriod;

        // Get baseband I/Q directly from stimulus manager (avoids RF aliasing!)
        double bb_i = 0.0, bb_q = 0.0;
        if (stimulusManager) {
            stimulusManager->getBasebandIQ(t, lo_freq, bb_i, bb_q);
        } else {
            // Simple tone: baseband = amp * exp(j * 2π * (rf-lo) * t)
            double baseband_freq = rf_freq - lo_freq;
            double phase = 2.0 * M_PI * baseband_freq * t;
            bb_i = rf_amp * std::cos(phase);
            bb_q = rf_amp * std::sin(phase);
        }

        // Add some noise for realism
        double noise_i = (rand() / (double)RAND_MAX - 0.5) * 1e-6;
        double noise_q = (rand() / (double)RAND_MAX - 0.5) * 1e-6;
        bb_i += noise_i;
        bb_q += noise_q;

        IQFrame frame;
        frame.timestamp_ns = static_cast<uint64_t>(t * 1e9);
        frame.sequence = static_cast<uint32_t>(i);
        frame.flags = 0;

        // Model each QSD channel (all same for functional model)
        for (int ch = 0; ch < 3; ++ch) {
            // Apply anti-alias LPF to baseband I/Q
            double filtered_i = applyLpf(bb_i, lpf_xi[ch], lpf_yi[ch]);
            double filtered_q = applyLpf(bb_q, lpf_xq[ch], lpf_yq[ch]);

            // Scale to ADC range (24-bit, +/-1.65V full scale)
            constexpr double adcScale = 8388607.0 / 1.65;
            frame.qsd[ch].i = static_cast<int32_t>(std::clamp(filtered_i * adcScale, -8388608.0, 8388607.0));
            frame.qsd[ch].q = static_cast<int32_t>(std::clamp(filtered_q * adcScale, -8388608.0, 8388607.0));
        }

        // Write to transport or collect locally
        if (opts.stream && transport) {
            auto err = transport->write(frame);
            if (err != TransportError::None && err != TransportError::BufferFull) {
                std::cerr << "Transport write error" << std::endl;
            }
        } else {
            frames.push_back(frame);
        }

        // Progress reporting
        if (opts.verbose && i % 9600 == 0 && i > 0) {
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - startTime).count();
            double progress = 100.0 * i / numSamples;
            double simTime = i * samplePeriod;
            double speed = simTime / elapsed;

            if (opts.stream) {
                std::cout << "\r[Stream] " << std::fixed << std::setprecision(1)
                          << progress << "% | "
                          << simTime * 1000 << " ms | "
                          << i << " samples | "
                          << transport->writeCount() << " written     " << std::flush;
            } else {
                std::cout << "\r[Functional] " << std::fixed << std::setprecision(1)
                          << progress << "% | "
                          << simTime * 1000 << " ms | "
                          << i << " samples | "
                          << speed << "x realtime     " << std::flush;
            }
        }

        // Real-time pacing when streaming (to not overwhelm buffer)
        if (opts.stream && i % 960 == 0) {  // Every 10ms of samples
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - startTime).count();
            double simTime = i * samplePeriod;

            if (simTime > elapsed) {
                std::this_thread::sleep_for(
                    std::chrono::microseconds(static_cast<int>((simTime - elapsed) * 1e6)));
            }

            // Check for LO frequency updates from app
            double new_lo;
            if (readControlFile(new_lo) && std::abs(new_lo - lo_freq) > 1.0) {  // >1Hz change
                lo_freq = new_lo;
                if (opts.verbose) {
                    std::cout << "\n[Control] LO changed to " << std::fixed << std::setprecision(6)
                              << lo_freq / 1e6 << " MHz" << std::endl;
                }
            }
        }
    }

    auto endTime = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(endTime - startTime).count();

    if (opts.verbose) {
        std::cout << "\r" << std::string(70, ' ') << "\r";
    }

    std::cout << "\n--- Results ---" << std::endl;
    if (opts.stream && transport) {
        std::cout << "Samples written: " << transport->writeCount() << std::endl;
        std::cout << "Overruns: " << transport->overruns() << std::endl;
    } else {
        std::cout << "Samples: " << frames.size() << std::endl;
    }
    std::cout << "Wall time: " << elapsed * 1000 << " ms" << std::endl;
    std::cout << "Speed: " << (opts.duration_ms / 1000.0) / elapsed << "x realtime" << std::endl;

    if (!opts.stream) {
        analyzeFrames(frames);
    }

    // Clean up transport
    if (transport) {
        transport->disconnect();
    }

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
