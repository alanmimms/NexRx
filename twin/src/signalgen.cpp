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
    double rf_freq_mhz = 14.201;
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
              << "  --rf FREQ        Set static RF signal frequency in MHz (default: 14.201)\n"
              << "  --lo FREQ        Set initial LO frequency in MHz (default: 14.200)\n"
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
    std::cout << "=== NexRx Digital Twin - FUNCTIONAL MODE ===" << std::endl;
    
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
    
    std::cout << "[Control] Listening on " << opts.controlPort << ". Waiting for client..." << std::endl;
    if (!control->acceptClient(std::chrono::milliseconds(0))) return 1;
    std::cout << "[Control] Client connected from " << control->peerAddress() << std::endl;

    UdpStreamConfig streamConfig; streamConfig.host = control->peerIP(); streamConfig.port = opts.streamPort; streamConfig.server = true;
    auto stream = std::make_unique<UdpStreamTransport>(streamConfig);
    if (!stream->connect()) return 1;

    double lo = opts.lo_freq_mhz * 1e6, k = opts.qsd_offset_khz * 1000.0;
    auto controlHandler = std::make_unique<ControlHandler>(lo - k, lo + k, lo, &attenuator, &preselector);
    controlHandler->start(control.get(), opts.verbose);

    constexpr double sampleRate = 96000.0, samplePeriod = 1.0 / sampleRate;
    const int OVERSAMPLE = 5;
    const double oversamplePeriod = samplePeriod / OVERSAMPLE;
    
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

    double lo_phase[3] = {0,0,0}, lo_phase_d[3], current_freqs[3];
    auto updatePhases = [&](int ch, double f) {
        lo_phase_d[ch] = 2.0 * M_PI * f * oversamplePeriod;
        current_freqs[ch] = f;
        memset(lpf_zi[ch], 0, sizeof(lpf_zi[ch]));
        memset(lpf_zq[ch], 0, sizeof(lpf_zq[ch]));
    };
    for(int ch=0; ch<3; ++ch) updatePhases(ch, ch==0 ? lo-k : (ch==1 ? lo+k : lo));

    // RF Tone State
    double rf_phase = 0.0, rf_phase_d;
    { rf_phase_d = 2.0 * M_PI * (opts.rf_freq_mhz * 1e6) * oversamplePeriod; }
    double rf_amp = opts.rf_amplitude_mv * 1e-3;

    // BIST State
    double bist_phase = 0.0, bist_phase_d = 0.0, bist_last_freq = -1.0;

    auto startTime = std::chrono::steady_clock::now();
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
            continue;
        }

        for (int step=0; step < 960; ++step) {
            double f_i[3], f_q[3];
            for (int os=0; os < OVERSAMPLE; ++os) {
                double t = (outputSample * OVERSAMPLE + os) * oversamplePeriod;
                double rf_i=0, rf_q=0;
                
                if (stimulusManager) {
                    stimulusManager->getRfIQ(t, rf_i, rf_q);
                } else {
                    rf_i = rf_amp * cos(rf_phase); rf_q = rf_amp * sin(rf_phase);
                    rf_phase = fmod(rf_phase + rf_phase_d, 2.0 * M_PI);
                }
                
                if (controlHandler->isBistEnabled()) {
                    double bf = controlHandler->getBistFreq();
                    if (std::abs(bf - bist_last_freq) > 1.0) {
                        bist_phase_d = 2.0 * M_PI * bf * oversamplePeriod;
                        bist_last_freq = bf;
                    }
                    rf_i += 0.000158 * cos(bist_phase); // ~158uV signal
                    bist_phase = fmod(bist_phase + bist_phase_d, 2.0 * M_PI);
                }

                double gain = attenuator.getVoltageGain();
                rf_i *= gain; rf_q *= gain;
                
                for (int ch=0; ch<3; ++ch) {
                    double c = cos(lo_phase[ch]), s = sin(lo_phase[ch]);
                    double bi = rf_i*c + rf_q*s, bq = rf_q*c - rf_i*s;
                    double fi = applyLpf(bi, lpf_zi[ch]), fq = applyLpf(bq, lpf_zq[ch]);
                    if (os == OVERSAMPLE - 1) { f_i[ch] = fi; f_q[ch] = fq; }
                    lo_phase[ch] = fmod(lo_phase[ch] + lo_phase_d[ch], 2.0 * M_PI);
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
        double simTime = outputSample * samplePeriod;
        double elapsed = std::chrono::duration<double>(now - startTime).count();
        if (simTime > elapsed) {
            std::this_thread::sleep_for(std::chrono::microseconds((int)((simTime - elapsed)*1e6)));
        }

        for (int ch=0; ch<3; ++ch) {
            double f = controlHandler->getQsdVfo(ch);
            if (std::abs(f - current_freqs[ch]) > 1.0) { updatePhases(ch, f); lo_phase[ch] = 0; }
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
