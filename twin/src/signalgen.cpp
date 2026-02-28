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
    bool no_stimulus = false;
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
              << "  --no-stimulus    Do not load any stimulus (silent RF)\n"
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
        else if (arg == "--quiet") opts.verbose = false;
        else if (arg == "--no-stimulus") opts.no_stimulus = true;
        else if (arg == "--rf" && i+1 < argc) opts.rf_freq_mhz = std::stod(argv[++i]);
        else if (arg == "--lo" && i+1 < argc) opts.lo_freq_mhz = std::stod(argv[++i]);
        else if (arg == "--amplitude" && i+1 < argc) opts.rf_amplitude_mv = std::stod(argv[++i]);
        else if (arg == "--qsd-offset" && i+1 < argc) opts.qsd_offset_khz = std::stod(argv[++i]);
        else if (arg == "--stimulus" && i+1 < argc) opts.stimulus = argv[++i];
        else if (arg == "--duration" && i+1 < argc) opts.duration_ms = std::stod(argv[++i]);
        else if (arg == "--port" && i+1 < argc) opts.controlPort = (uint16_t)std::stoi(argv[++i]);
        else {
            std::cerr << "Unknown option: " << arg << std::endl;
            printUsage(argv[0]);
            exit(1);
        }
    }
    return opts;
}

struct BiquadCoeffs { double b0, b1, b2, a1, a2; };
static constexpr BiquadCoeffs ak5578_480k_stages[3] = {
    {0.0006628600, 0.0008272571, 0.0006628600, -1.5472148716, 0.6157336719},
    {1.0000000000, -0.4832493140, 1.0000000000, -1.5609518734, 0.7359192796},
    {1.0000000000, -0.9740021692, 1.0000000000, -1.6237519754, 0.9064567688},
};

// Characterize filter rejection
double getPreselGain(double freq_hz, const PreselectorModel& model) {
    static const double cap_vals[11] = {
        10e-12, 20e-12, 40e-12, 80e-12, 160e-12, 320e-12, 
        640e-12, 1.28e-9, 2.56e-9, 5.12e-9, 10.24e-9
    };
    double total_c = 20e-12; // Stray capacitance
    for (int i=0; i<11; ++i) if (model.getCap(i)) total_c += cap_vals[i];
    double total_l = model.getInd(0) ? (1.5e-6 + 220e-9) : 220e-9;
    double f_res = 1.0 / (2.0 * M_PI * std::sqrt(total_l * total_c));
    double q = 40.0 * (1.0 - (f_res / 150e6));
    double bw = f_res / std::max(1.0, q);
    double rel_f = std::abs(freq_hz - f_res) / (bw / 2.0);
    double atten = 1.0 / std::sqrt(1.0 + std::pow(rel_f, 4.0));
    double leak = 0.0003; 
    double srf_dist = std::abs(freq_hz - 45e6) / 2e6;
    double srf_peak = 0.05 / (1.0 + srf_dist * srf_dist);
    return std::max({atten, leak, srf_peak});
}

int runFunctionalMode(const Options& opts) {
    std::cout << "=== NexRx Digital Twin - FUNCTIONAL MODE (High-Fidelity) ===" << std::endl;
    AttenuatorModel attenuator;
    PreselectorModel preselector;
    PgaModel pga;
    std::shared_ptr<StimulusManager> stimulusManager;
    std::unique_ptr<sol::state> lua;
    std::string stimulusPath = opts.stimulus.empty() ? "config/stimuli/default.lua" : opts.stimulus;

    if (!opts.no_stimulus && std::ifstream(stimulusPath).good()) {
        lua = std::make_unique<sol::state>();
        lua->open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);
        StimulusLua stimLua; stimLua.registerBindings(*lua);
        if (stimLua.loadScript(*lua, stimulusPath)) {
            stimulusManager = stimLua.manager();
            stimulusManager->freeze();
            if (opts.verbose) std::cout << "[Twin] Loaded stimulus: " << stimulusPath << std::endl;
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
    auto controlHandler = std::make_unique<ControlHandler>(lo - k, lo + k, lo, &attenuator, &preselector, &pga);
    controlHandler->start(control.get(), opts.verbose);

    constexpr double sampleRate = 96000.0, samplePeriod = 1.0 / sampleRate;
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
    auto streamStartTime = std::chrono::steady_clock::now();
    size_t outputSample = 0;
    std::vector<IQFrame> batch; batch.reserve(32);

    while (true) {
        if (!controlHandler->isConnected() || !controlHandler->isStreaming()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            if (controlHandler->isConnected()) {
                std::string newIP = controlHandler->consumeReconnect();
                if (!newIP.empty()) stream->setDestination(newIP, opts.streamPort);
            }
            streamStartTime = std::chrono::steady_clock::now();
            outputSample = 0;
            if (stimulusManager) stimulusManager->resetAll();
            continue;
        }

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - streamStartTime).count();
        auto target_elapsed = (outputSample * 1000000ULL) / 96000ULL;
        if (elapsed < target_elapsed) {
            std::this_thread::sleep_for(std::chrono::microseconds(target_elapsed - elapsed));
        }

        for (int step=0; step < 960; ++step) {
            static std::mt19937 rng(12345);
            static std::normal_distribution<double> dist(0, 1e-8); 
            double t = outputSample * samplePeriod;
            double attenGain = attenuator.getVoltageGain();
            double f_i[3], f_q[3];

            for (int ch=0; ch<3; ++ch) {
                double vfo = controlHandler->getQsdVfo(ch);
                double pgaGainI = std::pow(10.0, pga.getGain(ch * 2) / 20.0);
                double pgaGainQ = std::pow(10.0, pga.getGain(ch * 2 + 1) / 20.0);
                if (std::abs(vfo - current_vfos[ch]) > 0.1) {
                    current_vfos[ch] = vfo;
                    memset(lpf_zi[ch], 0, sizeof(lpf_zi[ch]));
                    memset(lpf_zq[ch], 0, sizeof(lpf_zq[ch]));
                }
                double rf_bb_i = 0, rf_bb_q = 0;
                if (stimulusManager) {
                    stimulusManager->getRfIQ(t, rf_bb_i, rf_bb_q, vfo, 400000.0);
                    double presel = getPreselGain(vfo, preselector);
                    rf_bb_i *= presel; rf_bb_q *= presel;
                } else if (opts.rf_amplitude_mv > 0) {
                    double rf_f = opts.rf_freq_mhz * 1e6;
                    double off = rf_f - vfo;
                    double p = 2.0 * M_PI * off * t;
                    double presel = getPreselGain(rf_f, preselector);
                    rf_bb_i = (opts.rf_amplitude_mv * 1e-3) * presel * std::cos(p);
                    rf_bb_q = (opts.rf_amplitude_mv * 1e-3) * presel * std::sin(p);
                }
                double isg_bb_i = 0, isg_bb_q = 0;
                if (controlHandler->isIsgEnabled()) {
                    double isg_f = controlHandler->getIsgFreq();
                    double isg_off = isg_f - vfo;
                    double p = 2.0 * M_PI * isg_off * t;
                    double isg_presel = getPreselGain(isg_f, preselector);
                    // 50uV constant level (approx S9 + 20dB)
                    isg_bb_i = 0.000050 * isg_presel * std::cos(p);
                    isg_bb_q = 0.000050 * isg_presel * std::sin(p);
                }
                double total_bb_i = rf_bb_i * attenGain + isg_bb_i + dist(rng);
                double total_bb_q = rf_bb_q * attenGain + isg_bb_q + dist(rng);
                double gainErr = 1.029, phaseErrRad = 0.026;
                double bi = total_bb_i;
                double bq = (total_bb_q * std::cos(phaseErrRad) - total_bb_i * std::sin(phaseErrRad)) * gainErr;
                f_i[ch] = applyLpf(bi * pgaGainI, lpf_zi[ch]);
                f_q[ch] = applyLpf(bq * pgaGainQ, lpf_zq[ch]);
            }
            IQFrame packet;
            packet.sequence = (uint32_t)outputSample;
            packet.timestamp_ns = outputSample * 10416;
            for (int ch=0; ch<3; ++ch) {
                packet.qsd[ch].i = (int32_t)(f_i[ch] * 8388607.0);
                packet.qsd[ch].q = (int32_t)(f_q[ch] * 8388607.0);
            }
            batch.push_back(packet);
            if (batch.size() >= 32) {
                stream->writeBatch(batch);
                batch.clear();
            }
            outputSample++;
        }
    }
    return 0;
}

int main(int argc, char* argv[]) {
#ifdef HAVE_SSE_DENORMAL_CONTROL
    _mm_setcsr(_mm_getcsr() | 0x8040);
#endif
    Options opts = parseArgs(argc, argv);
    if (opts.help) { printUsage(argv[0]); return 0; }
    return runFunctionalMode(opts);
}
