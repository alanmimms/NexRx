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
    bool no_cal = false;
    std::string cal_file = "";
    double duration_ms = 0.0;
    double rf_freq_mhz = 14.120; 
    double lo_freq_mhz = 14.200;
    double rf_amplitude_mv = 1.0;
    double qsd_offset_khz = 12.0;
    std::string stimulus = "";
    std::string bindAddr = "0.0.0.0";
    uint16_t controlPort = 5000;
    uint16_t streamPort = 5001;

    // Simulated Hardware Inaccuracies (per channel)
    double gainErr[3];      // Voltage ratio
    double phaseErrRad[3];  // Radians
};

void printUsage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n"
              << "Options:\n"
              << "  --help, -h       Show this help\n"
              << "  --quiet          Disable verbose command logging\n"
              << "  --no-stimulus    Do not load any stimulus (silent RF)\n"
              << "  --no-cal         Ignore calibration and use random errors\n"
              << "  --cal-file FILE  Load hardware calibration from JSON\n"
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
    for (int i=0; i<3; ++i) { opts.gainErr[i] = 1.0; opts.phaseErrRad[i] = 0.0; }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") opts.help = true;
        else if (arg == "--quiet") opts.verbose = false;
        else if (arg == "--no-stimulus") opts.no_stimulus = true;
        else if (arg == "--no-cal") opts.no_cal = true;
        else if (arg == "--cal-file" && i+1 < argc) opts.cal_file = argv[++i];
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

    if (opts.cal_file.empty() || opts.no_cal) {
        std::mt19937 rng(std::chrono::system_clock::now().time_since_epoch().count());
        std::uniform_real_distribution<double> gDist(0.95, 1.05);
        std::uniform_real_distribution<double> pDist(-0.05, 0.05);
        
        std::cout << "[Twin] Simulated Hardware Inaccuracies (Random):" << std::endl;
        std::cout << "Channel | Rejection | Simulated Gain Err | Simulated Phase Err" << std::endl;
        std::cout << "--------+-----------+--------------------+--------------------" << std::endl;
        for (int i=0; i<3; ++i) {
            opts.gainErr[i] = gDist(rng);
            opts.phaseErrRad[i] = pDist(rng);
            double g = opts.gainErr[i], p = opts.phaseErrRad[i];
            double rej_num = 1.0 + g*g + 2.0*g*std::cos(p);
            double rej_den = 1.0 + g*g - 2.0*g*std::cos(p);
            double rej = 10.0 * std::log10(rej_num / std::max(1e-10, rej_den));
            std::cout << std::setw(7) << i << " | " << std::fixed << std::setprecision(1) << std::setw(7) << rej << " dBc | " << std::setw(15) << std::setprecision(3) << 20.0*std::log10(g) << " dB | " << std::setw(15) << std::setprecision(2) << p * (180.0/M_PI) << " deg" << std::endl;
        }
        std::cout << std::endl;
    }
    return opts;
}

void getPreselParams(const PreselectorModel& model, double& f_res, double& q_factor) {
    // Physical capacitor values from C701-C712
    static const double cap_vals[11] = { 
        8e-12, 16e-12, 32e-12, 68e-12, 130e-12, 240e-12, 
        560e-12, 1000e-12, 2200e-12, 3900e-12, 8000e-12 
    };
    double total_c = 20e-12; for (int i=0; i<11; ++i) if (model.getCap(i)) total_c += cap_vals[i];
    // L=y shorts L1 (1.5uH), leaving L2 (220nH)
    double total_l = model.isL1Shorted() ? 220e-9 : (1.5e-6 + 220e-9);
    f_res = 1.0 / (2.0 * M_PI * std::sqrt(total_l * total_c));
    q_factor = 40.0 * (1.0 - (f_res / 150e6)); if (q_factor < 5.0) q_factor = 5.0;
}

double getPreselGain(double freq_hz, const PreselectorModel& model) {
    double f_res, q_factor; getPreselParams(model, f_res, q_factor);
    double bw = f_res / q_factor;
    double rel_f = std::abs(freq_hz - f_res) / (bw / 2.0);
    double atten = 1.0 / std::sqrt(1.0 + std::pow(rel_f, 4.0));
    double leak = 0.0003; 
    double srf_dist = std::abs(freq_hz - 45e6) / 2e6;
    double srf_peak = 0.05 / (1.0 + srf_dist * srf_dist);
    return std::max({atten, leak, srf_peak});
}

int runFunctionalMode(const Options& opts) {
    std::cout << "=== NexRx Digital Twin - FUNCTIONAL MODE (High-Fidelity) ===" << std::endl;
    AttenuatorModel attenuator; PreselectorModel preselector; PgaModel pga;
    std::shared_ptr<StimulusManager> stimulusManager;
    std::unique_ptr<sol::state> lua;
    std::string stimulusPath = opts.stimulus.empty() ? "config/stimuli/default.lua" : opts.stimulus;
    if (!opts.no_stimulus && std::ifstream(stimulusPath).good()) {
        lua = std::make_unique<sol::state>(); lua->open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);
        StimulusLua stimLua; stimLua.registerBindings(*lua);
        if (stimLua.loadScript(*lua, stimulusPath)) { stimulusManager = stimLua.manager(); stimulusManager->freeze(); if (opts.verbose) std::cout << "[Twin] Loaded stimulus: " << stimulusPath << std::endl; }
    }
    TcpControlConfig ctlConfig; ctlConfig.host = opts.bindAddr; ctlConfig.port = opts.controlPort; ctlConfig.server = true;
    auto control = std::make_unique<TcpControlTransport>(ctlConfig); if (!control->connect()) return 1; if (!control->acceptClient(std::chrono::milliseconds(0))) return 1;
    UdpStreamConfig streamConfig; streamConfig.host = control->peerIP(); streamConfig.port = opts.streamPort; streamConfig.server = true;
    auto stream = std::make_unique<UdpStreamTransport>(streamConfig); if (!stream->connect()) return 1;
    double lo = opts.lo_freq_mhz * 1e6, k = opts.qsd_offset_khz * 1000.0;
    auto controlHandler = std::make_unique<ControlHandler>(lo - k, lo + k, lo, &attenuator, &preselector, &pga); controlHandler->start(control.get(), opts.verbose);
    double sampleRate = 96000.0, samplePeriod = 1.0 / sampleRate;
    double isg_noise_zi[3][2] = {}, isg_noise_zq[3][2] = {};
    double current_vfos[3] = {0};
    auto streamStartTime = std::chrono::steady_clock::now();
    size_t outputSample = 0; std::vector<IQFrame> batch; batch.reserve(32);
    while (true) {
        int targetRate; std::vector<int> chMap;
        controlHandler->getCodecConfig(targetRate, chMap);
        if (std::abs(sampleRate - (double)targetRate) > 1.0) {
            sampleRate = (double)targetRate;
            samplePeriod = 1.0 / sampleRate;
            // Reset state on rate change
            memset(isg_noise_zi, 0, sizeof(isg_noise_zi));
            memset(isg_noise_zq, 0, sizeof(isg_noise_zq));
            streamStartTime = std::chrono::steady_clock::now();
            outputSample = 0;
            if (opts.verbose) std::cout << "[Twin] Sample rate changed to " << sampleRate << " Hz" << std::endl;
        }

        if (!controlHandler->isConnected() || !controlHandler->isStreaming()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            if (controlHandler->isConnected()) { std::string newIP = controlHandler->consumeReconnect(); if (!newIP.empty()) stream->setDestination(newIP, opts.streamPort); }
            streamStartTime = std::chrono::steady_clock::now(); outputSample = 0; if (stimulusManager) stimulusManager->resetAll();
            continue;
        }
        auto now = std::chrono::steady_clock::now(); auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - streamStartTime).count();
        auto target_elapsed = (uint64_t)(outputSample * 1000000.0 / sampleRate);
        if (elapsed < target_elapsed) { std::this_thread::sleep_for(std::chrono::microseconds(target_elapsed - elapsed)); }
        
        // Number of samples to process in this iteration (aim for ~10ms batches)
        int nSteps = (int)(sampleRate / 100.0);
        if (nSteps < 1) nSteps = 1;

        for (int step=0; step < nSteps; ++step) {
            static std::mt19937 rng(12345); static std::normal_distribution<double> dist(0, 1e-8); 
            double t = outputSample * samplePeriod; double attenGain = attenuator.getVoltageGain();
            double pgaGainLinear = std::pow(10.0, pga.getGain() / 20.0);
            double f_i[3] = {0}, f_q[3] = {0};
            for (int ch=0; ch<3; ++ch) {
                double vfo = controlHandler->getQsdVfo(ch);
                if (std::abs(vfo - current_vfos[ch]) > 0.1) { current_vfos[ch] = vfo; memset(isg_noise_zi[ch], 0, sizeof(isg_noise_zi[ch])); memset(isg_noise_zq[ch], 0, sizeof(isg_noise_zq[ch])); }
                double rf_bb_i = 0, rf_bb_q = 0;
                if (stimulusManager) { stimulusManager->getRfIQ(t, rf_bb_i, rf_bb_q, vfo, 1000000.0); double presel = getPreselGain(vfo, preselector); rf_bb_i *= presel; rf_bb_q *= presel; }
                else if (opts.rf_amplitude_mv > 0) { double rf_f = opts.rf_freq_mhz * 1e6, off = rf_f - vfo, p = 2.0 * M_PI * off * t, presel = getPreselGain(rf_f, preselector); rf_bb_i = (opts.rf_amplitude_mv * 1e-3) * presel * std::cos(p); rf_bb_q = (opts.rf_amplitude_mv * 1e-3) * presel * std::sin(p); }
                double isg_bb_i = 0, isg_bb_q = 0;
                if (controlHandler->isIsgEnabled()) {
                    double isg_f = controlHandler->getIsgFreq();
                    if (isg_f > 1.0) { double isg_off = isg_f - vfo, p = 2.0 * M_PI * isg_off * t, isg_presel = getPreselGain(isg_f, preselector); isg_bb_i = 0.000050 * isg_presel * std::cos(p); isg_bb_q = 0.000050 * isg_presel * std::sin(p); }
                    else if (isg_f == 1.0) {
                        static std::uniform_real_distribution<double> white(-1.0, 1.0);
                        double f_res_isg, q_isg; getPreselParams(preselector, f_res_isg, q_isg);
                        double f_off_isg = std::clamp(f_res_isg - vfo, -sampleRate/2.0, sampleRate/2.0), bw_isg = f_res_isg / q_isg;
                        // 1st order complex resonator: y = (1-r)*x + r*exp(j*theta)*y_prev
                        double r = std::exp(-M_PI * bw_isg / sampleRate);
                        double theta = 2.0 * M_PI * f_off_isg / sampleRate;
                        double re = r * std::cos(theta), im = r * std::sin(theta);
                        double x_i = white(rng), x_q = white(rng);
                        double y_prev_i = isg_noise_zi[ch][0], y_prev_q = isg_noise_zq[ch][0];
                        double y_i = (1.0 - r) * x_i + (re * y_prev_i - im * y_prev_q);
                        double y_q = (1.0 - r) * x_q + (re * y_prev_q + im * y_prev_i);
                        isg_noise_zi[ch][0] = y_i; isg_noise_zq[ch][0] = y_q;
                        isg_bb_i = 0.000050 * (y_i + x_i * 0.0003); isg_bb_q = 0.000050 * (y_q + x_q * 0.0003);
                    }
                }
                double t_bb_i = rf_bb_i * attenGain + isg_bb_i + dist(rng), t_bb_q = rf_bb_q * attenGain + isg_bb_q + dist(rng);
                double gE = opts.gainErr[ch], pE = opts.phaseErrRad[ch], bi = t_bb_i, bq = (t_bb_q * std::cos(pE) - t_bb_i * std::sin(pE)) * gE;
                f_i[ch] = bi * pgaGainLinear; f_q[ch] = bq * pgaGainLinear;
            }
            IQFrame pk; pk.sequence = (uint32_t)outputSample; pk.timestamp_ns = (uint64_t)(outputSample * 1e9 / sampleRate);
            for (int ch=0; ch<3; ++ch) { pk.qsd[ch].i = (int32_t)(f_i[ch] * 8388607.0); pk.qsd[ch].q = (int32_t)(f_q[ch] * 8388607.0); }
            batch.push_back(pk); if (batch.size() >= 32) { stream->writeBatch(batch); batch.clear(); }
            outputSample++;
        }
    }
    return 0;
}

int main(int argc, char* argv[]) {
#ifdef HAVE_SSE_DENORMAL_CONTROL
    _mm_setcsr(_mm_getcsr() | 0x8040);
#endif
    Options opts = parseArgs(argc, argv); if (opts.help) { printUsage(argv[0]); return 0; }
    return runFunctionalMode(opts);
}
