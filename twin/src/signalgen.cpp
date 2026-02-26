// NexRx Digital Twin
//
// Software-in-the-loop signal generator for the NexRx Triple-QSD SDR.
//
// Build target: twin
// Copyright 2026 NexRx Project - MIT License

#include "orchestrator/Orchestrator.hpp"
#include "ControlHandler.hpp"
#include "sampler/AdcSampler.hpp"
#include "sampler/RxControls.hpp"
#include "transport/IQFrame.hpp"
#include "transport/TcpControlTransport.hpp"
#include "transport/UdpStreamTransport.hpp"
#include "stimulus/ToneGenerator.hpp"
#include "stimulus/StimulusLua.hpp"
#include "AttenuatorModel.hpp"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <cmath>
#include <cstring>
#include <vector>
#include <chrono>
#include <thread>
#include <memory>
#include <random>
#include <atomic>
#include <mutex>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
#include <xmmintrin.h>
#include <pmmintrin.h>
#define HAVE_SSE_DENORMAL_CONTROL 1
#endif

using namespace nexrx;

struct Options {
    bool functional = true;
    bool help = false;
    bool verbose = true;
    bool stream = true;
    double duration_ms = 0.0;
    double rf_freq_mhz = 14.120; 
    double lo_freq_mhz = 14.200;
    double rf_amplitude_mv = 1.0;
    double qsd_offset_khz = 12.0;
    std::string stimulus = "";
    std::string bindAddr = "0.0.0.0";
    uint16_t controlPort = 5000;
    uint16_t streamPort = 5001;
};

void printUsage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n"
              << "Options:\n"
              << "  --help, -h       Show this help\n"
              << "  --quiet          Disable verbose command logging\n"
              << "  --rf FREQ        Set static RF signal frequency in MHz (default: 14.12)\n"
              << "  --lo FREQ        Set initial LO frequency in MHz (default: 14.20)\n"
              << "  --amplitude MV   Set RF signal amplitude in mV (default: 1.0)\n"
              << "  --qsd-offset KHZ Set QSD offset k in kHz (default: 12.0)\n"
              << "  --stimulus FILE  Load Lua stimulus script (overrides --rf)\n"
              << "  --duration MS    Run for specific duration in ms (default: forever)\n"
              << "  --port PORT      Set TCP control port (default: 5000)\n"
              << std::endl;
}

Options parseArgs(int argc, char* argv[]) {
    Options opts;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") opts.help = true;
        else if (arg == "--rf" && i+1 < argc) opts.rf_freq_mhz = std::stod(argv[++i]);
        else if (arg == "--lo" && i+1 < argc) opts.lo_freq_mhz = std::stod(argv[++i]);
        else if (arg == "--amplitude" && i+1 < argc) opts.rf_amplitude_mv = std::stod(argv[++i]);
        else if (arg == "--qsd-offset" && i+1 < argc) opts.qsd_offset_khz = std::stod(argv[++i]);
        else if (arg == "--stimulus" && i+1 < argc) opts.stimulus = argv[++i];
        else if (arg == "--duration" && i+1 < argc) opts.duration_ms = std::stod(argv[++i]);
        else if (arg == "--port" && i+1 < argc) opts.controlPort = (uint16_t)std::stoi(argv[++i]);
        else if (arg == "--quiet") opts.verbose = false;
    }
    return opts;
}

struct BiquadCoeffs { double b0, b1, b2, a1, a2; };
static constexpr BiquadCoeffs ak5578_480k_stages[3] = {
    {0.0006628600, 0.0008272571, 0.0006628600, -1.5472148716, 0.6157336719},
    {1.0000000000, -0.4832493140, 1.0000000000, -1.5609518734, 0.7359192796},
    {1.0000000000, -0.9740021692, 1.0000000000, -1.6237519754, 0.9064567688},
};

int runFunctionalMode(const Options& opts) {
    std::cout << "=== NexRx Digital Twin - FUNCTIONAL MODE (High-Fidelity) ===" << std::endl;
    
    AttenuatorModel attenuator;
    PreselectorModel preselector;
    std::shared_ptr<StimulusManager> stimulusManager;
    std::unique_ptr<sol::state> lua;
    std::string stimulusPath = opts.stimulus.empty() ? "config/stimuli/default.lua" : opts.stimulus;

    if (std::ifstream(stimulusPath).good()) {
        lua = std::make_unique<sol::state>();
        lua->open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);
        StimulusLua stimLua; stimLua.registerBindings(*lua);
        if (stimLua.loadScript(*lua, stimulusPath)) {
            stimulusManager = stimLua.manager();
            stimulusManager->freeze();
            std::cout << "[Twin] Loaded stimulus: " << stimulusPath << std::endl;
        }
    }

    TcpControlConfig ctlConfig; ctlConfig.host = opts.bindAddr; ctlConfig.port = opts.controlPort; ctlConfig.server = true;
    auto control = std::make_unique<TcpControlTransport>(ctlConfig);
    if (!control->connect()) return 1;
    
    if (!control->acceptClient(std::chrono::milliseconds(0))) return 1;

    UdpStreamConfig streamConfig; streamConfig.host = control->peerIP(); streamConfig.port = opts.streamPort; streamConfig.server = true;
    auto stream = std::make_unique<UdpStreamTransport>(streamConfig);
    if (!stream->connect()) return 1;

    double lo = opts.lo_freq_mhz * 1e6, k = opts.qsd_offset_khz * 1000.0;
    auto controlHandler = std::make_unique<ControlHandler>(lo - k, lo + k, lo, &attenuator, &preselector);
    controlHandler->start(control.get(), opts.verbose);

    constexpr double sampleRate = 96000.0, samplePeriod = 1.0 / sampleRate;
    const int OVERSAMPLE = 5;
    const double oversampleRate = sampleRate * OVERSAMPLE;
    const double oversamplePeriod = 1.0 / oversampleRate;
    
    double lpf_zi[3][3][2] = {}, lpf_zq[3][3][2] = {};
    auto applyLpf = [&](double x, double z[3][2]) {
        double y = x;
        for (int s=0; s<3; ++s) {
            const auto& c = ak5578_480k_stages[s];
            double out = c.b0 * y + z[s][0];
            z[s][0] = c.b1 * y - c.a1 * out + z[s][1];
            z[s][1] = c.b2 * y - c.a2 * out;
            y = out;
        }
        return y;
    };

    double current_vfos[3] = {0};
    double lo_phase[3] = {0, 0, 0};
    double bist_phase[3] = {0, 0, 0};
    double rf_phase = 0.0;
    double rf_target_hz = opts.rf_freq_mhz * 1e6;

    auto startTime = std::chrono::steady_clock::now();
    auto lastLogTime = startTime;
    size_t outputSample = 0;
    std::vector<IQFrame> batch; batch.reserve(32);

    while (true) {
        if (!controlHandler->isConnected() || !controlHandler->isStreaming()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            if (controlHandler->isConnected()) {
                std::string newIP = controlHandler->consumeReconnect();
                if (!newIP.empty()) stream->setDestination(newIP, opts.streamPort);
            }
            startTime = std::chrono::steady_clock::now();
            outputSample = 0;
            if (stimulusManager) stimulusManager->resetAll();
            for (int i=0; i<3; ++i) { lo_phase[i] = 0; bist_phase[i] = 0; }
            rf_phase = 0;
            continue;
        }

        double max_rf_val = 0;

        for (int step=0; step < 960; ++step) {
            double f_i[3] = {0}, f_q[3] = {0};
            for (int os=0; os < OVERSAMPLE; ++os) {
                double t = (outputSample * OVERSAMPLE + os) * oversamplePeriod;
                double gain = attenuator.getVoltageGain();

                // 1. Get ANALYTIC RF Antenna Signal (Composite of all stimuli)
                double rf_i = 0, rf_q = 0;
                if (stimulusManager) {
                    stimulusManager->getRfIQ(t, rf_i, rf_q);
                } else {
                    rf_phase = std::fmod(rf_phase + 2.0 * M_PI * rf_target_hz * oversamplePeriod, 2.0 * M_PI);
                    rf_i = (opts.rf_amplitude_mv * 1e-3) * std::cos(rf_phase);
                    rf_q = (opts.rf_amplitude_mv * 1e-3) * std::sin(rf_phase);
                }

                max_rf_val = std::max(max_rf_val, std::sqrt(rf_i*rf_i + rf_q*rf_q));

                // 2. Mix with 3 independent LOs
                for (int ch=0; ch<3; ++ch) {
                    double vfo = controlHandler->getQsdVfo(ch);
                    if (std::abs(vfo - current_vfos[ch]) > 0.1) {
                        current_vfos[ch] = vfo;
                        // Reset filters on tune to prevent transients
                        memset(lpf_zi[ch], 0, sizeof(lpf_zi[ch]));
                        memset(lpf_zq[ch], 0, sizeof(lpf_zq[ch]));
                    }

                    // Complex downconversion with phase accumulation
                    lo_phase[ch] = std::fmod(lo_phase[ch] + 2.0 * M_PI * vfo * oversamplePeriod, 2.0 * M_PI);
                    double cos_lo = std::cos(lo_phase[ch]);
                    double sin_lo = std::sin(lo_phase[ch]);
                    
                    double bi = rf_i * cos_lo + rf_q * sin_lo;
                    double bq = rf_q * cos_lo - rf_i * sin_lo; // Q is -90 deg shift

                    // 3. INTERNAL SIGNAL GENERATOR (ISG) - Injected at baseband
                    if (controlHandler->isBistEnabled()) {
                        double bist_f = controlHandler->getBistFreq();
                        double bist_offset = bist_f - vfo;
                        
                        bist_phase[ch] = std::fmod(bist_phase[ch] + 2.0 * M_PI * bist_offset * oversamplePeriod, 2.0 * M_PI);
                        
                        // ISG Rejection Filter: Reject signals outside simulation Nyquist (240 kHz)
                        // Uses a sharp 8th-order Butterworth-like curve centered at 200kHz.
                        double rel_f = std::abs(bist_offset) / 200000.0;
                        double rejection = 1.0 / (1.0 + std::pow(rel_f, 8.0));
                        
                        if (rejection > 0.0001) {
                            // 0.66uV constant level at ADC (matches hardware)
                            bi += (0.00000066 * rejection / gain) * std::cos(bist_phase[ch]);
                            bq += (0.00000066 * rejection / gain) * std::sin(bist_phase[ch]);
                        }
                    }

                    f_i[ch] = applyLpf(bi * gain, lpf_zi[ch]);
                    f_q[ch] = applyLpf(bq * gain, lpf_zq[ch]);
                }
            }

            IQFrame frame; frame.timestamp_ns = (uint64_t)(outputSample * samplePeriod * 1e9); frame.sequence = (uint32_t)outputSample;
            static thread_local std::mt19937 rng(std::random_device{}()); static thread_local std::uniform_real_distribution<double> dist(-0.5, 0.5);
            auto quant = [&](double v) { 
                double scaled = v * 8388607.0/1.65;
                if (!std::isfinite(scaled)) return (int32_t)0;
                return (int32_t)std::clamp(std::round(scaled + dist(rng)+dist(rng)), -8388608.0, 8388607.0); 
            };
            for (int ch=0; ch<3; ++ch) { frame.qsd[ch].i = quant(f_i[ch]); frame.qsd[ch].q = quant(f_q[ch]); }
            batch.push_back(frame);
            if (batch.size() >= 32) { stream->writeBatch(batch); batch.clear(); }
            outputSample++;
        }

        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration<double>(now - lastLogTime).count() > 1.0) {
            if (opts.verbose) {
                std::cout << "[Twin] Max RF Input: " << std::fixed << std::setprecision(1) << max_rf_val * 1e6 << " uV" << std::endl;
            }
            lastLogTime = now;
        }

        double simTime = outputSample * samplePeriod;
        double elapsed = std::chrono::duration<double>(now - startTime).count();
        if (simTime > elapsed) {
            std::this_thread::sleep_for(std::chrono::microseconds((int)((simTime - elapsed)*1e6)));
        }
    }
    return 0;
}

int main(int argc, char* argv[]) {
#ifdef HAVE_SSE_DENORMAL_CONTROL
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON); _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#endif
    Options opts = parseArgs(argc, argv);
    if (opts.help) { printUsage(argv[0]); return 0; }
    return runFunctionalMode(opts);
}
