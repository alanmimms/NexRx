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
    // Default errors (0)
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

    // Generate random inaccuracies if no calibration
    if (opts.cal_file.empty() || opts.no_cal) {
        std::mt19937 rng(std::chrono::system_clock::now().time_since_epoch().count());
        std::uniform_real_distribution<double> gDist(0.95, 1.05); // +/- 5% gain
        std::uniform_real_distribution<double> pDist(-0.05, 0.05); // +/- ~3 deg phase
        
        std::cout << "[Twin] Simulated Hardware Inaccuracies (Random):" << std::endl;
        std::cout << "Channel | Rejection | Simulated Gain Err | Simulated Phase Err" << std::endl;
        std::cout << "--------+-----------+--------------------+--------------------" << std::endl;
        
        for (int i=0; i<3; ++i) {
            opts.gainErr[i] = gDist(rng);
            opts.phaseErrRad[i] = pDist(rng);
            
            // Calculate theoretical rejection (IRR)
            double g = opts.gainErr[i];
            double p = opts.phaseErrRad[i];
            double rej_num = 1.0 + g*g + 2.0*g*std::cos(p);
            double rej_den = 1.0 + g*g - 2.0*g*std::cos(p);
            double rej = 10.0 * std::log10(rej_num / std::max(1e-10, rej_den));

            std::cout << std::setw(7) << i << " | "
                      << std::fixed << std::setprecision(1) << std::setw(7) << rej << " dBc | "
                      << std::setw(15) << std::setprecision(3) << 20.0*std::log10(g) << " dB | "
                      << std::setw(15) << std::setprecision(2) << p * (180.0/M_PI) << " deg" << std::endl;
        }
        std::cout << std::endl;
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
    // Corrected capacitor values (C701-C711)
    static const double cap_vals[11] = {
        8e-12, 16e-12, 32e-12, 68e-12, 130e-12, 240e-12, 
        560e-12, 1000e-12, 2200e-12, 3300e-12, 8000e-12
    };
    double total_c = 20e-12; // Stray capacitance floor
    for (int i=0; i<11; ++i) if (model.getCap(i)) total_c += cap_vals[i];
    
    // L1 (1.5uH) is shorted when L=y (model.isL1Shorted() == true)
    // L2 (220nH) is always in circuit.
    double total_l = model.isL1Shorted() ? 220e-9 : (1.5e-6 + 220e-9);
    
    double f_res = 1.0 / (2.0 * M_PI * std::sqrt(total_l * total_c));
    
    // Quality factor - declines with frequency
    double q = 40.0 * (1.0 - (f_res / 150e6));
    double bw = f_res / std::max(1.0, q);
    
    // Parallel LC Tank response (Bandpass)
    double rel_f = std::abs(freq_hz - f_res) / (bw / 2.0);
    double atten = 1.0 / std::sqrt(1.0 + std::pow(rel_f, 4.0));
    
    // Artifacts: -70dB leakage floor, SRF peak at 45MHz
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
    double isg_noise_zi[3][2] = {}, isg_noise_zq[3][2] = {};
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
                    if (isg_f > 1.0) {
                        double isg_off = isg_f - vfo;
                        double p = 2.0 * M_PI * isg_off * t;
                        double isg_presel = getPreselGain(isg_f, preselector);
                        // 50uV constant level (approx S9 + 20dB)
                        isg_bb_i = 0.000050 * isg_presel * std::cos(p);
                        isg_bb_q = 0.000050 * isg_presel * std::sin(p);
                    } else if (isg_f == 1.0) {
                        static std::uniform_real_distribution<double> white(-1.0, 1.0);
                        
                        // Use exact resonance parameters from centralized helper
                        double total_c_noise = 20e-12; // Base stray
                        static const double cap_vals_isg[11] = { 8e-12, 16e-12, 32e-12, 68e-12, 130e-12, 240e-12, 560e-12, 1000e-12, 2200e-12, 3300e-12, 8000e-12 };
                        for (int i=0; i<11; ++i) if (preselector.getCap(i)) total_c_noise += cap_vals_isg[i];
                        double total_l_noise = preselector.isL1Shorted() ? 220e-9 : (1.5e-6 + 220e-9);
                        
                        double f_res_isg = 1.0 / (2.0 * M_PI * std::sqrt(total_l_noise * total_c_noise));
                        double q_isg = 40.0 * (1.0 - (f_res_isg / 150e6));
                        double f_off_isg = f_res_isg - vfo;
                        
                        double r = 1.0 - (M_PI * (f_res_isg / q_isg) / sampleRate);
                        double theta = 2.0 * M_PI * f_off_isg / sampleRate;
                        double a1 = -2.0 * r * std::cos(theta);
                        double a2 = r * r;
                        double g_res = (1.0 - r * r) * 0.5;

                        auto applyResonator = [&](double x, double z[2]) {
                            double out = g_res * x - a1 * z[0] - a2 * z[1];
                            z[1] = z[0]; z[0] = out;
                            return out;
                        };

                        double raw_i = white(rng), raw_q = white(rng);
                        isg_bb_i = 0.000050 * (applyResonator(raw_i, isg_noise_zi[ch]) + raw_i * 0.0003);
                        isg_bb_q = 0.000050 * (applyResonator(raw_q, isg_noise_zq[ch]) + raw_q * 0.0003);
                    }
                }
                double total_bb_i = rf_bb_i * attenGain + isg_bb_i + dist(rng);
                double total_bb_q = rf_bb_q * attenGain + isg_bb_q + dist(rng);
                double gainErr = opts.gainErr[ch], phaseErrRad = opts.phaseErrRad[ch];
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
