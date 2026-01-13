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
#include "transport/TcpControlTransport.hpp"
#include "transport/UdpStreamTransport.hpp"
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
    bool stream = false;          // Stream I/Q to network for app display
    bool spectrum = false;        // Spectrum analysis diagnostic mode
    double spectrum_rate_khz = 500.0;  // Sample rate for spectrum analysis (kHz)
    double duration_ms = 0.0;     // Simulation duration in ms (0 = run forever)
    double rf_freq_mhz = 14.201;  // RF signal frequency (1kHz above LO for USB)
    double lo_freq_mhz = 14.200;  // LO frequency (matches app default VFO)
    double rf_amplitude_mv = 1.0; // RF amplitude in mV
    double qsd_offset_khz = 12.0; // QSD offset k in kHz (f-k, f+k for image rejection)
    std::string netlist = "netlists/pipeline_test.cir";
    std::string stimulus = "";    // Stimulus script (empty = use simple tone)
    std::string bindAddr = "0.0.0.0";  // Address to bind (0.0.0.0 = all interfaces)
    std::string udpHost = "";         // Override UDP destination (empty = use TCP peer IP)
    uint16_t controlPort = 5000;  // TCP control port
    uint16_t streamPort = 5001;   // UDP stream port
};

//=============================================================================
// Attenuator Model - 4 cascaded T-pad attenuators (3, 6, 12, 24 dB)
//=============================================================================
class AttenuatorModel {
public:
    // Enable/disable individual attenuators
    void setAtten3dB(bool enabled) { atten3dB_.store(enabled, std::memory_order_relaxed); }
    void setAtten6dB(bool enabled) { atten6dB_.store(enabled, std::memory_order_relaxed); }
    void setAtten12dB(bool enabled) { atten12dB_.store(enabled, std::memory_order_relaxed); }
    void setAtten24dB(bool enabled) { atten24dB_.store(enabled, std::memory_order_relaxed); }

    bool getAtten3dB() const { return atten3dB_.load(std::memory_order_relaxed); }
    bool getAtten6dB() const { return atten6dB_.load(std::memory_order_relaxed); }
    bool getAtten12dB() const { return atten12dB_.load(std::memory_order_relaxed); }
    bool getAtten24dB() const { return atten24dB_.load(std::memory_order_relaxed); }

    // Get total attenuation in dB
    double getTotalDb() const {
        double total = 0.0;
        if (atten3dB_.load(std::memory_order_relaxed))  total += 3.0;
        if (atten6dB_.load(std::memory_order_relaxed))  total += 6.0;
        if (atten12dB_.load(std::memory_order_relaxed)) total += 12.0;
        if (atten24dB_.load(std::memory_order_relaxed)) total += 24.0;
        return total;
    }

    // Get voltage gain (< 1.0 when attenuation enabled)
    double getVoltageGain() const {
        double db = getTotalDb();
        return std::pow(10.0, -db / 20.0);
    }

    // Set total attenuation, picking closest valid combination
    void setTotalDb(double target_db) {
        // Valid values: 0, 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 42, 45
        // Round to nearest 3dB step
        int steps = static_cast<int>(std::round(target_db / 3.0));
        steps = std::max(0, std::min(15, steps));  // 0-15 (0-45 dB)
        double actual_db = steps * 3.0;

        // Decompose into individual attenuators (greedy from largest)
        atten24dB_.store(actual_db >= 24.0, std::memory_order_relaxed);
        if (actual_db >= 24.0) actual_db -= 24.0;

        atten12dB_.store(actual_db >= 12.0, std::memory_order_relaxed);
        if (actual_db >= 12.0) actual_db -= 12.0;

        atten6dB_.store(actual_db >= 6.0, std::memory_order_relaxed);
        if (actual_db >= 6.0) actual_db -= 6.0;

        atten3dB_.store(actual_db >= 3.0, std::memory_order_relaxed);
    }

private:
    std::atomic<bool> atten3dB_{false};
    std::atomic<bool> atten6dB_{false};
    std::atomic<bool> atten12dB_{false};
    std::atomic<bool> atten24dB_{false};
};

void printUsage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n"
              << "\n"
              << "Modes:\n"
              << "  --functional    Fast C++ functional model (real-time capable)\n"
              << "  (default)       Full Xyce SPICE physics simulation (slow but accurate)\n"
              << "\n"
              << "Options:\n"
              << "  --stream        Stream I/Q to network for app display\n"
              << "  --bind ADDR     Bind address (default: 0.0.0.0 = all interfaces)\n"
              << "  --udp-host ADDR Override UDP destination (default: TCP peer IP)\n"
              << "  --control-port  TCP control port (default: 5000)\n"
              << "  --stream-port   UDP stream port (default: 5001)\n"
              << "  --stimulus FILE Lua stimulus script (default: config/stimuli/default.lua)\n"
              << "  --duration MS   Simulation duration in milliseconds (0 = forever, default)\n"
              << "  --netlist FILE  Xyce netlist path (physics mode only)\n"
              << "  --rf FREQ       RF frequency in MHz (for simple tone, no stimulus script)\n"
              << "  --lo FREQ       LO frequency in MHz (default: 14.200)\n"
              << "  --amplitude MV  RF amplitude in mV (for simple tone, default: 1.0)\n"
              << "  --qsd-offset K  QSD offset k in kHz for image rejection (default: 12.0)\n"
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
        } else if (strcmp(argv[i], "--qsd-offset") == 0 && i + 1 < argc) {
            opts.qsd_offset_khz = std::atof(argv[++i]);
        } else if (strcmp(argv[i], "--stimulus") == 0 && i + 1 < argc) {
            opts.stimulus = argv[++i];
        } else if (strcmp(argv[i], "--bind") == 0 && i + 1 < argc) {
            opts.bindAddr = argv[++i];
        } else if (strcmp(argv[i], "--udp-host") == 0 && i + 1 < argc) {
            opts.udpHost = argv[++i];
        } else if (strcmp(argv[i], "--control-port") == 0 && i + 1 < argc) {
            opts.controlPort = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (strcmp(argv[i], "--stream-port") == 0 && i + 1 < argc) {
            opts.streamPort = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (strcmp(argv[i], "--spectrum") == 0) {
            opts.spectrum = true;
        } else if (strcmp(argv[i], "--spectrum-rate") == 0 && i + 1 < argc) {
            opts.spectrum_rate_khz = std::atof(argv[++i]);
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
// TCP Control Handler - runs in separate thread
//=============================================================================
class ControlHandler {
public:
    ControlHandler(double initial_lo_hz, double initial_k_khz, AttenuatorModel* atten = nullptr)
        : lo_freq_hz_(initial_lo_hz), qsd_offset_khz_(initial_k_khz),
          attenuator_(atten), streaming_(false), running_(false) {}

    void start(TcpControlTransport* control, bool verbose) {
        control_ = control;
        verbose_ = verbose;
        running_ = true;
        thread_ = std::thread(&ControlHandler::run, this);
    }

    void stop() {
        running_ = false;
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    double getLO() const { return lo_freq_hz_.load(std::memory_order_relaxed); }
    void setLO(double freq) { lo_freq_hz_.store(freq, std::memory_order_relaxed); }
    double getQsdOffset() const { return qsd_offset_khz_.load(std::memory_order_relaxed); }
    void setQsdOffset(double k_khz) { qsd_offset_khz_.store(k_khz, std::memory_order_relaxed); }
    bool isStreaming() const { return streaming_.load(std::memory_order_relaxed); }
    void setStreaming(bool s) { streaming_.store(s, std::memory_order_relaxed); }

private:
    void run() {
        while (running_) {
            auto result = control_->receiveRequest(std::chrono::milliseconds(100));
            if (!result.ok()) {
                continue;  // Timeout or error, keep trying
            }

            std::string request(result.value.begin(), result.value.end());
            std::string response = handleCommand(request);

            std::vector<uint8_t> resp(response.begin(), response.end());
            control_->sendResponse(resp);
        }
    }

    std::string handleCommand(const std::string& cmd) {
        // Parse command
        std::istringstream iss(cmd);
        std::string verb;
        iss >> verb;

        if (verb == "SET_LO") {
            double freq;
            if (iss >> freq) {
                lo_freq_hz_.store(freq, std::memory_order_relaxed);
                if (verbose_) {
                    std::cout << "\n[Control] LO set to " << std::fixed << std::setprecision(6)
                              << freq / 1e6 << " MHz" << std::endl;
                }
                return "OK\n";
            }
            return "ERROR invalid frequency\n";
        }
        else if (verb == "GET_STATUS") {
            std::ostringstream oss;
            oss << "STATUS lo=" << std::fixed << std::setprecision(1) << lo_freq_hz_.load()
                << " streaming=" << (streaming_.load() ? "true" : "false") << "\n";
            return oss.str();
        }
        else if (verb == "START_STREAM") {
            streaming_.store(true, std::memory_order_relaxed);
            return "OK\n";
        }
        else if (verb == "STOP_STREAM") {
            streaming_.store(false, std::memory_order_relaxed);
            return "OK\n";
        }
        else if (verb == "GET_STREAM_CONFIG") {
            // Return stream configuration for rate negotiation
            // IQ rate is fixed at 96kHz, 3 QSD channels
            std::ostringstream oss;
            oss << "STREAM_CONFIG iq_rate=96000 channels=3 format=cbor\n";
            return oss.str();
        }
        else if (verb == "SET_QSD_OFFSET") {
            double k_khz;
            if (iss >> k_khz) {
                qsd_offset_khz_.store(k_khz, std::memory_order_relaxed);
                if (verbose_) {
                    std::cout << "\n[Control] QSD offset set to " << k_khz << " kHz" << std::endl;
                }
                return "OK\n";
            }
            return "ERROR invalid offset\n";
        }
        else if (verb == "GET_QSD_OFFSET") {
            std::ostringstream oss;
            oss << "QSD_OFFSET " << std::fixed << std::setprecision(3)
                << qsd_offset_khz_.load() << "\n";
            return oss.str();
        }
        else if (verb == "SET_ATTEN" && attenuator_) {
            // SET_ATTEN <3|6|12|24> <0|1> - set individual attenuator
            int db;
            int enabled;
            if (iss >> db >> enabled) {
                bool en = (enabled != 0);
                switch (db) {
                    case 3:  attenuator_->setAtten3dB(en); break;
                    case 6:  attenuator_->setAtten6dB(en); break;
                    case 12: attenuator_->setAtten12dB(en); break;
                    case 24: attenuator_->setAtten24dB(en); break;
                    default: return "ERROR invalid attenuator (use 3, 6, 12, or 24)\n";
                }
                if (verbose_) {
                    std::cout << "\n[Control] Attenuator " << db << "dB: "
                              << (en ? "ON" : "OFF") << " (total: "
                              << attenuator_->getTotalDb() << " dB)" << std::endl;
                }
                return "OK\n";
            }
            return "ERROR usage: SET_ATTEN <3|6|12|24> <0|1>\n";
        }
        else if (verb == "SET_ATTEN_TOTAL" && attenuator_) {
            // SET_ATTEN_TOTAL <dB> - set total attenuation (rounds to nearest valid)
            double db;
            if (iss >> db) {
                attenuator_->setTotalDb(db);
                if (verbose_) {
                    std::cout << "\n[Control] Attenuation set to "
                              << attenuator_->getTotalDb() << " dB" << std::endl;
                }
                return "OK\n";
            }
            return "ERROR usage: SET_ATTEN_TOTAL <dB>\n";
        }
        else if (verb == "GET_ATTEN" && attenuator_) {
            std::ostringstream oss;
            oss << "ATTEN total=" << std::fixed << std::setprecision(1)
                << attenuator_->getTotalDb()
                << " 3dB=" << (attenuator_->getAtten3dB() ? "1" : "0")
                << " 6dB=" << (attenuator_->getAtten6dB() ? "1" : "0")
                << " 12dB=" << (attenuator_->getAtten12dB() ? "1" : "0")
                << " 24dB=" << (attenuator_->getAtten24dB() ? "1" : "0") << "\n";
            return oss.str();
        }

        return "ERROR unknown command\n";
    }

    TcpControlTransport* control_ = nullptr;
    bool verbose_ = false;
    std::atomic<double> lo_freq_hz_;
    std::atomic<double> qsd_offset_khz_;
    AttenuatorModel* attenuator_ = nullptr;
    std::atomic<bool> streaming_;
    std::atomic<bool> running_;
    std::thread thread_;
};

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

            // Freeze stimuli for lock-free access during streaming
            stimulusManager->freeze();
        }
    }

    if (!useStimulus) {
        std::cout << "RF: " << opts.rf_freq_mhz << " MHz, " << opts.rf_amplitude_mv << " mV" << std::endl;
        std::cout << "Baseband: " << std::abs(opts.rf_freq_mhz - opts.lo_freq_mhz) * 1000 << " kHz" << std::endl;
    }

    std::cout << "LO: " << opts.lo_freq_mhz << " MHz (tunable via TCP control)" << std::endl;
    std::cout << "QSD offset (k): " << opts.qsd_offset_khz << " kHz (tunable via TCP control)" << std::endl;
    std::cout << "Attenuator: 0-45 dB in 3dB steps (controllable via TCP)" << std::endl;
    if (runForever) {
        std::cout << "Duration: forever (Ctrl+C to stop)" << std::endl;
    } else {
        std::cout << "Duration: " << opts.duration_ms << " ms" << std::endl;
    }
    if (opts.stream) {
        std::cout << "Streaming: TCP:" << opts.controlPort << " UDP:" << opts.streamPort << std::endl;
    }
    std::cout << std::endl;

    const double rf_freq = opts.rf_freq_mhz * 1e6;
    double lo_freq = opts.lo_freq_mhz * 1e6;  // Initial LO - updated via TCP control
    const double rf_amp = opts.rf_amplitude_mv * 1e-3;  // Convert to volts

    constexpr double sampleRate = 96000.0;
    constexpr double samplePeriod = 1.0 / sampleRate;
    const double duration_s = runForever ? 1e9 : opts.duration_ms / 1000.0;  // ~31 years if forever
    const size_t numSamples = runForever ? SIZE_MAX : static_cast<size_t>(duration_s * sampleRate);

    // Attenuator model (shared with control handler)
    AttenuatorModel attenuator;

    // Set up network transports if streaming
    std::unique_ptr<TcpControlTransport> control;
    std::unique_ptr<UdpStreamTransport> stream;
    std::unique_ptr<ControlHandler> controlHandler;

    if (opts.stream) {
        // Create TCP control transport (server mode)
        TcpControlConfig ctlConfig;
        ctlConfig.host = opts.bindAddr;
        ctlConfig.port = opts.controlPort;
        ctlConfig.server = true;

        control = std::make_unique<TcpControlTransport>(ctlConfig);
        if (!control->connect()) {
            std::cerr << "Failed to bind TCP control on " << opts.bindAddr
                      << ":" << opts.controlPort << std::endl;
            return 1;
        }
        std::cout << "[Control] Listening on " << opts.bindAddr
                  << ":" << opts.controlPort << std::endl;

        // Wait for client connection
        std::cout << "[Control] Waiting for client connection..." << std::endl;
        if (!control->acceptClient(std::chrono::milliseconds(0))) {  // 0 = wait forever
            std::cerr << "Failed to accept client connection" << std::endl;
            return 1;
        }

        // Get client IP for UDP streaming
        std::string clientIP = control->peerIP();
        std::cout << "[Control] Client connected from " << control->peerAddress() << std::endl;

        // Determine UDP destination: override if specified, else use TCP peer IP
        std::string udpDest = opts.udpHost.empty() ? clientIP : opts.udpHost;
        if (!opts.udpHost.empty()) {
            std::cout << "[Stream] UDP destination override: " << opts.udpHost << std::endl;
        }

        // Create UDP stream transport (sender mode)
        UdpStreamConfig streamConfig;
        streamConfig.host = udpDest;
        streamConfig.port = opts.streamPort;
        streamConfig.server = true;  // Sender mode
        streamConfig.framesPerPacket = 32;

        stream = std::make_unique<UdpStreamTransport>(streamConfig);
        if (!stream->connect()) {
            std::cerr << "Failed to create UDP stream" << std::endl;
            return 1;
        }
        std::cout << "[Stream] Sending UDP to " << udpDest
                  << ":" << opts.streamPort << std::endl;

        // Start control handler thread
        controlHandler = std::make_unique<ControlHandler>(lo_freq, opts.qsd_offset_khz, &attenuator);
        controlHandler->start(control.get(), opts.verbose);
        controlHandler->setStreaming(true);
    }

    std::vector<IQFrame> frames;
    if (!opts.stream && !runForever) {
        frames.reserve(numSamples);
    }

    // Batch buffer for streaming - reduces mutex lock overhead from 96K/sec to 3K/sec
    constexpr size_t BATCH_SIZE = 32;  // Match framesPerPacket in transport
    std::vector<IQFrame> batchBuffer;
    if (opts.stream) {
        batchBuffer.reserve(BATCH_SIZE);
    }

    auto startTime = std::chrono::steady_clock::now();

    // =======================================================================
    // AK5578 SHARP ROLL-OFF Digital Filter Emulation with OVERSAMPLING
    // =======================================================================
    // The real AK5578 has an analog anti-alias filter before its ADC.
    // To properly simulate this, we oversample at 480 kHz (5x), apply
    // the digital anti-alias filter, then decimate to 96 kHz output.
    //
    // This prevents aliasing of signals above 48 kHz - a signal at 96 kHz
    // baseband offset is properly attenuated (-86 dB) before decimation.
    //
    // Filter: 6th-order elliptic, fc=40kHz, fs=480kHz
    // Response:
    //   0-40 kHz: -0.1 dB (passband)
    //   44 kHz:   -4.9 dB (transition)
    //   48 kHz:  -14.2 dB
    //   96 kHz:  -86.2 dB (excellent image rejection!)
    //
    // Coefficients: scipy.signal.ellip(6, 0.1, 80, 40000, fs=480000)
    // =======================================================================

    // Biquad coefficients for 6th-order elliptic LPF at 480 kHz
    struct BiquadCoeffs {
        double b0, b1, b2, a1, a2;
    };

    static constexpr int NUM_LPF_STAGES = 3;
    static constexpr int OVERSAMPLE_RATIO = 5;  // 480 kHz / 96 kHz = 5

    // Filter coefficients for fs=480kHz, fc=40kHz
    static constexpr BiquadCoeffs ak5578_480k_stages[NUM_LPF_STAGES] = {
        {0.0006628600, 0.0008272571, 0.0006628600, -1.5472148716, 0.6157336719},
        {1.0000000000, -0.4832493140, 1.0000000000, -1.5609518734, 0.7359192796},
        {1.0000000000, -0.9740021692, 1.0000000000, -1.6237519754, 0.9064567688},
    };

    // Filter state for each QSD channel (I and Q), 3 stages each
    // [channel][stage][z1,z2]
    double lpf_zi[3][NUM_LPF_STAGES][2] = {};
    double lpf_zq[3][NUM_LPF_STAGES][2] = {};

    // Apply 6th-order elliptic LPF (3 cascaded biquads) - transposed direct form II
    auto applyLpf = [&](double x, double z[NUM_LPF_STAGES][2]) -> double {
        double y = x;
        for (int s = 0; s < NUM_LPF_STAGES; ++s) {
            const auto& c = ak5578_480k_stages[s];
            double out = c.b0 * y + z[s][0];
            z[s][0] = c.b1 * y - c.a1 * out + z[s][1];
            z[s][1] = c.b2 * y - c.a2 * out;
            y = out;
        }
        return y;
    };

    // QSD offset k (can be updated via TCP control)
    double k_hz = opts.qsd_offset_khz * 1000.0;

    // Oversampled processing: run filter at 480 kHz, output at 96 kHz
    const double oversamplePeriod = samplePeriod / OVERSAMPLE_RATIO;  // 1/480000
    size_t outputSample = 0;

    // =======================================================================
    // Incremental phase rotation for fast LO generation
    // =======================================================================
    // Instead of computing sin/cos every sample (expensive), we use:
    //   cos(θ + Δ) = cos(θ)cos(Δ) - sin(θ)sin(Δ)
    //   sin(θ + Δ) = sin(θ)cos(Δ) + cos(θ)sin(Δ)
    // This is much faster: 4 muls + 2 adds vs expensive trig calls
    // =======================================================================

    // Phase increments per oversample (at 480 kHz rate)
    auto computePhaseInc = [&](double freq_hz) -> std::pair<double, double> {
        double delta = 2.0 * M_PI * freq_hz * oversamplePeriod;
        return {std::cos(delta), std::sin(delta)};
    };

    // Current LO phases (cos, sin pairs) for each QSD
    double lo0_cos = 1.0, lo0_sin = 0.0;  // QSD0: f - k
    double lo1_cos = 1.0, lo1_sin = 0.0;  // QSD1: f + k
    double lo2_cos = 1.0, lo2_sin = 0.0;  // QSD2: f (center)

    // Phase increment (cos_delta, sin_delta) for each LO
    auto [lo0_cos_d, lo0_sin_d] = computePhaseInc(lo_freq - k_hz);
    auto [lo1_cos_d, lo1_sin_d] = computePhaseInc(lo_freq + k_hz);
    auto [lo2_cos_d, lo2_sin_d] = computePhaseInc(lo_freq);

    // For RF tone (when no stimulus)
    double rf_cos = 1.0, rf_sin = 0.0;
    auto [rf_cos_d, rf_sin_d] = computePhaseInc(rf_freq);

    // Fast xorshift PRNG for noise generation (much faster than rand())
    uint32_t rng_state = 12345;
    auto fastRand = [&rng_state]() -> double {
        rng_state ^= rng_state << 13;
        rng_state ^= rng_state >> 17;
        rng_state ^= rng_state << 5;
        return (rng_state / 4294967296.0) - 0.5;  // -0.5 to +0.5
    };

    for (size_t i = 0; i < numSamples * OVERSAMPLE_RATIO; ++i) {
        double t = i * oversamplePeriod;

        // Get RF I/Q from stimulus (antenna signal - knows nothing about LO)
        double rf_i = 0.0, rf_q = 0.0;
        if (stimulusManager) {
            stimulusManager->getRfIQ(t, rf_i, rf_q);
        } else {
            // Simple tone at RF frequency using incremental rotation
            rf_i = rf_amp * rf_cos;
            rf_q = rf_amp * rf_sin;
            // Rotate phase: (cos, sin) * (cos_d, sin_d)
            double new_cos = rf_cos * rf_cos_d - rf_sin * rf_sin_d;
            double new_sin = rf_sin * rf_cos_d + rf_cos * rf_sin_d;
            rf_cos = new_cos;
            rf_sin = new_sin;
        }

        // Apply attenuator (before mixers, models RF front-end attenuators)
        double atten_gain = attenuator.getVoltageGain();
        rf_i *= atten_gain;
        rf_q *= atten_gain;

        // Add some noise for realism (scaled for oversampling)
        double noise_i = fastRand() * 1e-6;
        double noise_q = fastRand() * 1e-6;

        // Mix and filter all 3 QSD channels at 480 kHz using incremental LO
        // QSD0: f - k
        double bb0_i = rf_i * lo0_cos + rf_q * lo0_sin + noise_i;
        double bb0_q = rf_q * lo0_cos - rf_i * lo0_sin + noise_q;
        double filt0_i = applyLpf(bb0_i, lpf_zi[0]);
        double filt0_q = applyLpf(bb0_q, lpf_zq[0]);

        // QSD1: f + k
        double bb1_i = rf_i * lo1_cos + rf_q * lo1_sin + noise_i;
        double bb1_q = rf_q * lo1_cos - rf_i * lo1_sin + noise_q;
        double filt1_i = applyLpf(bb1_i, lpf_zi[1]);
        double filt1_q = applyLpf(bb1_q, lpf_zq[1]);

        // QSD2: f (center)
        double bb2_i = rf_i * lo2_cos + rf_q * lo2_sin + noise_i;
        double bb2_q = rf_q * lo2_cos - rf_i * lo2_sin + noise_q;
        double filt2_i = applyLpf(bb2_i, lpf_zi[2]);
        double filt2_q = applyLpf(bb2_q, lpf_zq[2]);

        // Rotate all LO phases for next sample
        {
            double c = lo0_cos * lo0_cos_d - lo0_sin * lo0_sin_d;
            double s = lo0_sin * lo0_cos_d + lo0_cos * lo0_sin_d;
            lo0_cos = c; lo0_sin = s;
        }
        {
            double c = lo1_cos * lo1_cos_d - lo1_sin * lo1_sin_d;
            double s = lo1_sin * lo1_cos_d + lo1_cos * lo1_sin_d;
            lo1_cos = c; lo1_sin = s;
        }
        {
            double c = lo2_cos * lo2_cos_d - lo2_sin * lo2_sin_d;
            double s = lo2_sin * lo2_cos_d + lo2_cos * lo2_sin_d;
            lo2_cos = c; lo2_sin = s;
        }

        // Decimate: output every 5th sample (96 kHz output rate)
        if (i % OVERSAMPLE_RATIO == OVERSAMPLE_RATIO - 1) {
            IQFrame frame;
            frame.timestamp_ns = static_cast<uint64_t>(outputSample * samplePeriod * 1e9);
            frame.sequence = static_cast<uint32_t>(outputSample);
            frame.flags = 0;

            constexpr double adcScale = 8388607.0 / 1.65;
            frame.qsd[0].i = static_cast<int32_t>(std::clamp(filt0_i * adcScale, -8388608.0, 8388607.0));
            frame.qsd[0].q = static_cast<int32_t>(std::clamp(filt0_q * adcScale, -8388608.0, 8388607.0));
            frame.qsd[1].i = static_cast<int32_t>(std::clamp(filt1_i * adcScale, -8388608.0, 8388607.0));
            frame.qsd[1].q = static_cast<int32_t>(std::clamp(filt1_q * adcScale, -8388608.0, 8388607.0));
            frame.qsd[2].i = static_cast<int32_t>(std::clamp(filt2_i * adcScale, -8388608.0, 8388607.0));
            frame.qsd[2].q = static_cast<int32_t>(std::clamp(filt2_q * adcScale, -8388608.0, 8388607.0));

            // Write to transport or collect locally
            if (opts.stream && stream) {
                // Batch writes to reduce mutex lock overhead
                batchBuffer.push_back(frame);
                if (batchBuffer.size() >= BATCH_SIZE) {
                    auto err = stream->writeBatch(batchBuffer);
                    if (err != TransportError::None && err != TransportError::BufferFull) {
                        std::cerr << "Transport write error" << std::endl;
                    }
                    batchBuffer.clear();
                }
            } else {
                frames.push_back(frame);
            }

            outputSample++;
        }

        // Progress reporting (every 500ms of output samples = 48000 output * 5 oversample)
        // Reduced from 100ms to 500ms to minimize I/O overhead during streaming
        if (opts.verbose && i % (48000 * OVERSAMPLE_RATIO) == 0 && i > 0) {
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - startTime).count();
            double progress = 100.0 * outputSample / numSamples;
            double simTime = outputSample * samplePeriod;
            double speed = simTime / elapsed;

            if (opts.stream && stream) {
                // Show realtime ratio to diagnose slowdowns
                std::cout << "\r[Stream] " << std::fixed << std::setprecision(2)
                          << speed << "x | "
                          << simTime * 1000 << " ms | "
                          << stream->framesSent() << " sent     " << std::flush;
            } else {
                std::cout << "\r[Functional] " << std::fixed << std::setprecision(1)
                          << progress << "% | "
                          << simTime * 1000 << " ms | "
                          << outputSample << " samples | "
                          << speed << "x realtime     " << std::flush;
            }
        }

        // Real-time pacing when streaming (every 10ms of output = 960 * 5 oversample)
        if (opts.stream && i % (960 * OVERSAMPLE_RATIO) == 0) {
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - startTime).count();
            double simTime = outputSample * samplePeriod;

            if (simTime > elapsed) {
                std::this_thread::sleep_for(
                    std::chrono::microseconds(static_cast<int>((simTime - elapsed) * 1e6)));
            }

            // Check for LO frequency and k offset updates from TCP control
            if (controlHandler) {
                double new_lo = controlHandler->getLO();
                double new_k = controlHandler->getQsdOffset() * 1000.0;  // kHz to Hz

                // If LO or k changed, recalculate phase increments
                if (std::abs(new_lo - lo_freq) > 1.0 || std::abs(new_k - k_hz) > 1.0) {
                    lo_freq = new_lo;
                    k_hz = new_k;

                    // Recalculate phase increments for new frequencies
                    std::tie(lo0_cos_d, lo0_sin_d) = computePhaseInc(lo_freq - k_hz);
                    std::tie(lo1_cos_d, lo1_sin_d) = computePhaseInc(lo_freq + k_hz);
                    std::tie(lo2_cos_d, lo2_sin_d) = computePhaseInc(lo_freq);

                    // Reset phases to avoid discontinuity (start fresh)
                    lo0_cos = 1.0; lo0_sin = 0.0;
                    lo1_cos = 1.0; lo1_sin = 0.0;
                    lo2_cos = 1.0; lo2_sin = 0.0;
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
    if (opts.stream && stream) {
        std::cout << "Frames sent: " << stream->framesSent() << std::endl;
        std::cout << "Packets sent: " << stream->packetsSent() << std::endl;
    } else {
        std::cout << "Samples: " << frames.size() << std::endl;
    }
    std::cout << "Wall time: " << elapsed * 1000 << " ms" << std::endl;
    std::cout << "Speed: " << (opts.duration_ms / 1000.0) / elapsed << "x realtime" << std::endl;

    if (!opts.stream) {
        analyzeFrames(frames);
    }

    // Clean up transports
    if (controlHandler) {
        controlHandler->stop();
    }
    if (stream) {
        stream->flush();  // Send any remaining frames
        stream->disconnect();
    }
    if (control) {
        control->disconnect();
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
// Spectrum Analysis Mode
// Diagnostic tool to analyze signal spectrum at various sample rates
// to understand aliasing and image issues
//=============================================================================
int runSpectrumAnalysis(const Options& opts) {
    std::cout << "=== Spectrum Analysis Mode ===" << std::endl;

    double lo_freq = opts.lo_freq_mhz * 1e6;
    double rf_freq = opts.rf_freq_mhz * 1e6;
    double rf_amp = opts.rf_amplitude_mv * 1e-3;
    double sampleRate = opts.spectrum_rate_khz * 1000.0;
    double k_hz = opts.qsd_offset_khz * 1000.0;

    std::cout << "LO frequency:     " << opts.lo_freq_mhz << " MHz" << std::endl;
    std::cout << "Sample rate:      " << opts.spectrum_rate_khz << " kHz" << std::endl;
    std::cout << "QSD offset (k):   " << opts.qsd_offset_khz << " kHz" << std::endl;
    std::cout << "Nyquist:          " << opts.spectrum_rate_khz / 2.0 << " kHz" << std::endl;
    std::cout << std::endl;

    // Load stimulus
    std::shared_ptr<StimulusManager> stimulusManager;
    std::unique_ptr<sol::state> lua;

    bool useStimulus = !opts.stimulus.empty();
    if (useStimulus) {
        lua = std::make_unique<sol::state>();
        lua->open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);

        StimulusLua stimLua;
        stimLua.registerBindings(*lua);

        stimulusManager = stimLua.manager();

        try {
            lua->script_file(opts.stimulus);
            std::cout << "Loaded stimulus: " << opts.stimulus << std::endl;
        } catch (const sol::error& e) {
            std::cerr << "Stimulus error: " << e.what() << std::endl;
            return 1;
        }
    } else {
        std::cout << "Using simple tone at " << opts.rf_freq_mhz << " MHz" << std::endl;
    }
    (void)k_hz;  // Suppress unused warning

    // FFT size - use power of 2 for efficiency
    constexpr size_t FFT_SIZE = 8192;
    double duration_s = FFT_SIZE / sampleRate;
    double samplePeriod = 1.0 / sampleRate;

    std::cout << "FFT size:         " << FFT_SIZE << " samples" << std::endl;
    std::cout << "Duration:         " << duration_s * 1000.0 << " ms" << std::endl;
    std::cout << "Freq resolution:  " << sampleRate / FFT_SIZE << " Hz" << std::endl;
    std::cout << std::endl;

    // Collect baseband I/Q samples at the specified rate (NO anti-alias filter)
    std::vector<double> bb_i_raw(FFT_SIZE), bb_q_raw(FFT_SIZE);
    std::vector<double> bb_i_filt(FFT_SIZE), bb_q_filt(FFT_SIZE);

    // Also collect QSD2 (center frequency) baseband
    std::vector<double> qsd2_i(FFT_SIZE), qsd2_q(FFT_SIZE);

    std::cout << "Generating " << FFT_SIZE << " samples..." << std::endl;

    // Debug: check first few raw RF samples from stimulus
    double maxRfMag = 0.0;
    double sumRfMag = 0.0;

    for (size_t i = 0; i < FFT_SIZE; ++i) {
        double t = i * samplePeriod;

        // Get RF signal
        double rf_i = 0.0, rf_q = 0.0;
        if (stimulusManager) {
            stimulusManager->getRfIQ(t, rf_i, rf_q);
        } else {
            double phase = 2.0 * M_PI * rf_freq * t;
            rf_i = rf_amp * std::cos(phase);
            rf_q = rf_amp * std::sin(phase);
        }

        // Track RF signal magnitude for debug
        double rfMag = std::sqrt(rf_i * rf_i + rf_q * rf_q);
        if (rfMag > maxRfMag) maxRfMag = rfMag;
        sumRfMag += rfMag;

        // Mix to baseband at LO frequency (QSD2 - center)
        double lo_phase = 2.0 * M_PI * lo_freq * t;
        double cos_lo = std::cos(lo_phase);
        double sin_lo = std::sin(lo_phase);

        bb_i_raw[i] = rf_i * cos_lo + rf_q * sin_lo;
        bb_q_raw[i] = rf_q * cos_lo - rf_i * sin_lo;
        qsd2_i[i] = bb_i_raw[i];
        qsd2_q[i] = bb_q_raw[i];
    }

    // Debug output: RF signal statistics
    std::cout << "\nRF Signal Statistics:" << std::endl;
    std::cout << "  Max RF magnitude: " << maxRfMag * 1e6 << " µV" << std::endl;
    std::cout << "  Avg RF magnitude: " << (sumRfMag / FFT_SIZE) * 1e6 << " µV" << std::endl;

    // Compute magnitude spectrum using DFT
    auto computeSpectrum = [&](const std::vector<double>& sig_i,
                               const std::vector<double>& sig_q,
                               const std::string& label) {
        std::cout << "\n--- " << label << " Spectrum ---" << std::endl;

        // Find peaks in spectrum
        std::vector<std::pair<double, double>> peaks;  // (freq_hz, magnitude_db)

        // Check specific frequency bins - include expected signal frequencies
        // two_tone at 14.12 MHz with USB 700/1900 Hz tones should appear at +700 and +1900 Hz
        // BUG CHECK: If SSB formula is wrong, they might appear at -700 and -1900 Hz
        // ssb_tone at 14.15 MHz with USB 1000 Hz tone should appear at +30000 + 1000 = 31000 Hz
        std::vector<double> checkFreqs = {
            // Primary signal frequencies
            0, 500, 700, 1000, 1500, 1900, 2000, 3000, 5000,
            // Check for 96kHz offset images (the suspected problem)
            95000, 96000, 97000, 98000, 99000, 100000,
            -95000, -96000, -97000, -98000, -99000,
            // Other frequencies of interest
            30000, 31000, 45000, 48000, 50000,
            // Negative audio frequencies
            -500, -700, -1000, -1500, -1900, -2000
        };

        for (double freq : checkFreqs) {
            if (std::abs(freq) > sampleRate / 2) continue;

            // DFT at specific frequency (negative freq maps to bins N/2..N-1)
            double bin = freq * FFT_SIZE / sampleRate;
            if (bin < 0) bin += FFT_SIZE;  // Wrap negative to upper half
            int bin_int = static_cast<int>(std::round(bin));
            if (bin_int < 0 || bin_int >= (int)FFT_SIZE) continue;

            // Compute DFT bin
            double sum_r = 0.0, sum_i = 0.0;
            for (size_t n = 0; n < FFT_SIZE; ++n) {
                double angle = 2.0 * M_PI * bin_int * n / FFT_SIZE;
                // For complex input: X[k] = sum(x[n] * exp(-j*2*pi*k*n/N))
                // x[n] = i[n] + j*q[n]
                sum_r += sig_i[n] * std::cos(angle) + sig_q[n] * std::sin(angle);
                sum_i += -sig_i[n] * std::sin(angle) + sig_q[n] * std::cos(angle);
            }

            double mag = std::sqrt(sum_r * sum_r + sum_i * sum_i) / FFT_SIZE;
            double mag_db = 20.0 * std::log10(mag + 1e-15);

            if (mag_db > -120.0) {  // Show all measurable bins
                printf("  %8.1f Hz: %6.1f dB\n", freq, mag_db);
            }
        }

        // Also scan for any peaks above -100dB (very sensitive)
        std::cout << "\n  Scanning for peaks > -100dB..." << std::endl;
        double peakThreshold = -100.0;

        for (int bin = 0; bin < (int)FFT_SIZE/2; ++bin) {  // Every bin
            double sum_r = 0.0, sum_i = 0.0;
            for (size_t n = 0; n < FFT_SIZE; ++n) {
                double angle = 2.0 * M_PI * bin * n / FFT_SIZE;
                sum_r += sig_i[n] * std::cos(angle) + sig_q[n] * std::sin(angle);
                sum_i += -sig_i[n] * std::sin(angle) + sig_q[n] * std::cos(angle);
            }

            double mag = std::sqrt(sum_r * sum_r + sum_i * sum_i) / FFT_SIZE;
            double mag_db = 20.0 * std::log10(mag + 1e-15);
            double freq_hz = bin * sampleRate / FFT_SIZE;

            if (mag_db > peakThreshold) {
                printf("  PEAK: %8.1f Hz (%6.2f kHz): %6.1f dB\n",
                       freq_hz, freq_hz / 1000.0, mag_db);
            }
        }

        // Also check negative frequencies (for complex signal, -Nyquist to 0)
        std::cout << "\n  Checking negative frequencies..." << std::endl;
        for (int bin = (int)FFT_SIZE/2; bin < (int)FFT_SIZE; bin += 10) {
            double sum_r = 0.0, sum_i = 0.0;
            for (size_t n = 0; n < FFT_SIZE; ++n) {
                double angle = 2.0 * M_PI * bin * n / FFT_SIZE;
                sum_r += sig_i[n] * std::cos(angle) + sig_q[n] * std::sin(angle);
                sum_i += -sig_i[n] * std::sin(angle) + sig_q[n] * std::cos(angle);
            }

            double mag = std::sqrt(sum_r * sum_r + sum_i * sum_i) / FFT_SIZE;
            double mag_db = 20.0 * std::log10(mag + 1e-15);
            double freq_hz = (bin - (int)FFT_SIZE) * sampleRate / FFT_SIZE;  // Negative freq

            if (mag_db > peakThreshold) {
                printf("  PEAK: %8.1f Hz (%6.2f kHz): %6.1f dB\n",
                       freq_hz, freq_hz / 1000.0, mag_db);
            }
        }
    };

    // Analyze baseband spectrum
    computeSpectrum(qsd2_i, qsd2_q, "Baseband I/Q (mixed at LO, no filter)");

    std::cout << "\n=== Analysis Complete ===" << std::endl;
    std::cout << "Expected signal at: " << (rf_freq - lo_freq) / 1000.0 << " kHz" << std::endl;
    std::cout << "If you see signal at other frequencies, that indicates:" << std::endl;
    std::cout << "  - DC (0 Hz): carrier leakage or DC offset" << std::endl;
    std::cout << "  - Negative freq: image from imperfect mixing" << std::endl;
    std::cout << "  - fs-f: aliasing from inadequate anti-alias filter" << std::endl;

    return 0;
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

    if (opts.spectrum) {
        return runSpectrumAnalysis(opts);
    } else if (opts.functional) {
        return runFunctionalMode(opts);
    } else {
        return runPhysicsMode(opts);
    }
}
