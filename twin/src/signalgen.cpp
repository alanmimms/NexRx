// NexRx Digital Twin
//
// Software-in-the-loop signal generator for the NexRx Triple-QSD SDR.
//
// Build target: twin
// Copyright 2026 NexRx Project - MIT License

#include "orchestrator/Orchestrator.hpp"
#include "control-handler.hpp"
#include "sampler/AdcSampler.hpp"
#include "sampler/RxControls.hpp"
#include "transport/IQFrame.hpp"
#include "transport/tcp-control-transport.hpp"
#include "transport/udp-stream-transport.hpp"
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

namespace nexrx {

struct Options {
    bool functional = true;
    bool help = false;
    bool verbose = true;
    bool stream = true;
    bool noStimulus = false;
    bool noCal = false;
    std::string calFile = "";
    double durationMs = 0.0;
    double rfFreqMHz = 14.120; 
    double loFreqMHz = 14.200;
    double rfAmplitudeMV = 1.0;
    double qsdOffsetKHz = 12.0;
    std::string stimulus = "";
    std::string bindAddr = "0.0.0.0";
    uint16_t controlPort = 5000;
    uint16_t streamPort = 5001;

    double gainErr[3];
    double phaseErrRad[3];
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
        else if (arg == "--no-stimulus") opts.noStimulus = true;
        else if (arg == "--no-cal") opts.noCal = true;
        else if (arg == "--cal-file" && i+1 < argc) opts.calFile = argv[++i];
        else if (arg == "--rf" && i+1 < argc) opts.rfFreqMHz = std::stod(argv[++i]);
        else if (arg == "--lo" && i+1 < argc) opts.loFreqMHz = std::stod(argv[++i]);
        else if (arg == "--amplitude" && i+1 < argc) opts.rfAmplitudeMV = std::stod(argv[++i]);
        else if (arg == "--qsd-offset" && i+1 < argc) opts.qsdOffsetKHz = std::stod(argv[++i]);
        else if (arg == "--stimulus" && i+1 < argc) opts.stimulus = argv[++i];
        else if (arg == "--duration" && i+1 < argc) opts.durationMs = std::stod(argv[++i]);
        else if (arg == "--port" && i+1 < argc) opts.controlPort = (uint16_t)std::stoi(argv[++i]);
        else {
            std::cerr << "Unknown option: " << arg << std::endl;
            printUsage(argv[0]);
            exit(1);
        }
    }

    if (opts.calFile.empty() || opts.noCal) {
        std::mt19937 rng(static_cast<unsigned int>(std::chrono::system_clock::now().time_since_epoch().count()));
        std::uniform_real_distribution<double> gDist(0.95, 1.05);
        std::uniform_real_distribution<double> pDist(-0.05, 0.05);
        
        std::cout << "[Twin] Simulated Hardware Inaccuracies (Random):" << std::endl;
        std::cout << "Channel | Rejection | Simulated Gain Err | Simulated Phase Err" << std::endl;
        std::cout << "--------+-----------+--------------------+--------------------" << std::endl;
        for (int i=0; i<3; ++i) {
            opts.gainErr[i] = gDist(rng);
            opts.phaseErrRad[i] = pDist(rng);
            double g = opts.gainErr[i], p = opts.phaseErrRad[i];
            double rejNum = 1.0 + g*g + 2.0*g*std::cos(p);
            double rejDen = 1.0 + g*g - 2.0*g*std::cos(p);
            double rej = 10.0 * std::log10(rejNum / std::max(1e-10, rejDen));
            std::cout << std::setw(7) << i << " | " << std::fixed << std::setprecision(1) << std::setw(7) << rej << " dBc | " << std::setw(15) << std::setprecision(3) << 20.0*std::log10(g) << " dB | " << std::setw(15) << std::setprecision(2) << p * (180.0/M_PI) << " deg" << std::endl;
        }
        std::cout << std::endl;
    }
    return opts;
}

void getPreselParams(const PreselectorModel& model, double& fRes, double& qFactor) {
    static const double capVals[11] = { 
        8e-12, 16e-12, 32e-12, 68e-12, 130e-12, 240e-12, 
        560e-12, 1000e-12, 2200e-12, 3900e-12, 8000e-12 
    };
    double totalC = 20e-12; for (int i=0; i<11; ++i) if (model.getCap(i)) totalC += capVals[i];
    double totalL = model.isL1Shorted() ? 220e-9 : (1.5e-6 + 220e-9);
    fRes = 1.0 / (2.0 * M_PI * std::sqrt(totalL * totalC));
    qFactor = 40.0 * (1.0 - (fRes / 150e6)); if (qFactor < 5.0) qFactor = 5.0;
}

double getPreselGain(double freqHz, const PreselectorModel& model) {
    double fRes, qFactor; getPreselParams(model, fRes, qFactor);
    double bw = fRes / qFactor;
    double relF = std::abs(freqHz - fRes) / (bw / 2.0);
    double atten = 1.0 / std::sqrt(1.0 + std::pow(relF, 4.0));
    double leak = 0.0003; 
    double srfDist = std::abs(freqHz - 45e6) / 2e6;
    double srfPeak = 0.05 / (1.0 + srfDist * srfDist);
    return std::max({atten, leak, srfPeak});
}

int runFunctionalMode(const Options& opts) {
    std::cout << "=== NexRx Digital Twin - FUNCTIONAL MODE (Hardware Parity) ===" << std::endl;
    AttenuatorModel attenuator; PreselectorModel preselector; PGAModel pga;
    std::shared_ptr<StimulusManager> stimulusManager;
    std::unique_ptr<sol::state> lua;
    std::string stimulusPath = opts.stimulus.empty() ? "config/stimuli/default.lua" : opts.stimulus;
    if (!opts.noStimulus && std::ifstream(stimulusPath).good()) {
        lua = std::make_unique<sol::state>(); lua->open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);
        StimulusLua stimLua; stimLua.registerBindings(*lua);
        if (stimLua.loadScript(*lua, stimulusPath)) { stimulusManager = stimLua.manager(); stimulusManager->freeze(); if (opts.verbose) std::cout << "[Twin] Loaded stimulus: " << stimulusPath << std::endl; }
    }
    
    TCPControlConfig ctlConfig; 
    ctlConfig.host = opts.bindAddr; 
    ctlConfig.port = opts.controlPort; 
    ctlConfig.server = true;
    auto control = std::make_unique<TCPControlTransport>(ctlConfig);
    if (!control->connect()) return 1; 
    if (!control->acceptClient(std::chrono::milliseconds(0))) return 1;
    
    UPDStreamConfig streamConfig; 
    streamConfig.host = control->peerIP(); 
    streamConfig.port = opts.streamPort; 
    streamConfig.server = true;
    auto stream = std::make_unique<UPDStreamTransport>(streamConfig);
    if (!stream->connect()) return 1;
    
    double lo = opts.loFreqMHz * 1e6, k = opts.qsdOffsetKHz * 1000.0;
    auto controlHandler = std::make_unique<ControlHandler>(lo - k, lo + k, lo, &attenuator, &preselector, &pga);
    controlHandler->start(control.get(), opts.verbose);
    
    double sampleRate = 96000.0, samplePeriod = 1.0 / sampleRate;
    double isgNoiseZi[3][2] = {}, isgNoiseZq[3][2] = {};
    double currentVFOs[3] = {0};
    auto streamStartTime = std::chrono::steady_clock::now();
    size_t outputSample = 0; std::vector<IQFrame> batch; batch.reserve(32);
    
    while (true) {
        int targetRate; std::vector<int> chMap;
        controlHandler->getCodecConfig(targetRate, chMap);
        if (std::abs(sampleRate - (double)targetRate) > 1.0) {
            sampleRate = (double)targetRate; samplePeriod = 1.0 / sampleRate;
            memset(isgNoiseZi, 0, sizeof(isgNoiseZi)); memset(isgNoiseZq, 0, sizeof(isgNoiseZq));
            streamStartTime = std::chrono::steady_clock::now(); outputSample = 0;
        }

        if (!controlHandler->isConnected() || !controlHandler->isStreaming()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            streamStartTime = std::chrono::steady_clock::now(); outputSample = 0;
            if (stimulusManager) stimulusManager->resetAll();
            continue;
        }
        
        auto now = std::chrono::steady_clock::now(); 
        auto targetElapsed = static_cast<uint64_t>(outputSample * 1000000.0 / sampleRate);
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - streamStartTime).count();
        if (elapsed < static_cast<long long>(targetElapsed)) {
            std::this_thread::sleep_for(std::chrono::microseconds(targetElapsed - elapsed));
        }
        
        int nSteps = static_cast<int>(sampleRate / 100.0);
        if (nSteps < 1) nSteps = 1;

        for (int step=0; step < nSteps; ++step) {
            static std::mt19937 rng(12345); static std::normal_distribution<double> dist(0, 1e-8); 
            double t = outputSample * samplePeriod; double attenGain = attenuator.getVoltageGain();
            double pgaGainLinear = std::pow(10.0, pga.getGainDB() / 20.0);
            double fI[3] = {0}, fQ[3] = {0};
            
            for (int ch=0; ch<3; ++ch) {
                double vfo = controlHandler->getQSDFreq(ch);
                if (std::abs(vfo - currentVFOs[ch]) > 0.1) {
                    currentVFOs[ch] = vfo;
                    memset(isgNoiseZi[ch], 0, sizeof(isgNoiseZi[ch]));
                    memset(isgNoiseZq[ch], 0, sizeof(isgNoiseZq[ch]));
                }
                double rfBBi = 0, rfBBq = 0;
                if (stimulusManager) {
                    stimulusManager->getRfIQ(t, rfBBi, rfBBq, vfo, 1000000.0);
                    double presel = getPreselGain(vfo, preselector);
                    rfBBi *= presel; rfBBq *= presel;
                }
                double isgBBi = 0, isgBBq = 0;
                if (controlHandler->isISGEnabled()) {
                    double isgF = controlHandler->getISGFreq();
                    if (isgF > 1.0) {
                        double off = isgF - vfo, p = 2.0 * M_PI * off * t;
                        double presel = getPreselGain(isgF, preselector);
                        isgBBi = 0.000050 * presel * std::cos(p); isgBBq = 0.000050 * presel * std::sin(p);
                    } else if (isgF == 1.0) {
                        static std::uniform_real_distribution<double> white(-1.0, 1.0);
                        double fResISG, qISG; getPreselParams(preselector, fResISG, qISG);
                        double fOffISG = std::clamp(fResISG - vfo, -sampleRate/2.0, sampleRate/2.0);
                        double bwISG = fResISG / qISG;
                        double r = std::exp(-M_PI * bwISG / sampleRate);
                        double theta = 2.0 * M_PI * fOffISG / sampleRate;
                        double re = r * std::cos(theta), im = r * std::sin(theta);
                        double xI = white(rng), xQ = white(rng);
                        double yPrevI = isgNoiseZi[ch][0], yPrevQ = isgNoiseZq[ch][0];
                        double yI = (1.0 - r) * xI + (re * yPrevI - im * yPrevQ);
                        double yQ = (1.0 - r) * xQ + (re * yPrevQ + im * yPrevI);
                        isgNoiseZi[ch][0] = yI; isgNoiseZq[ch][0] = yQ;
                        isgBBi = 0.000050 * (yI + xI * 0.0003); isgBBq = 0.000050 * (yQ + xQ * 0.0003);
                    }
                }
                double tBBi = rfBBi * attenGain + isgBBi + dist(rng);
                double tBBq = rfBBq * attenGain + isgBBq + dist(rng);
                double gE = opts.gainErr[ch], pE = opts.phaseErrRad[ch];
                double bi = tBBi, bq = (tBBq * std::cos(pE) - tBBi * std::sin(pE)) * gE;
                fI[ch] = bi * pgaGainLinear; fQ[ch] = bq * pgaGainLinear;
            }
            
            IQFrame pk; pk.sequence = (uint32_t)outputSample;
            pk.timestamp_ns = static_cast<uint64_t>(outputSample * 1e9 / sampleRate);
            for (int ch=0; ch<3; ++ch) {
                pk.qsd[ch].i = static_cast<int32_t>(fI[ch] * 8388607.0);
                pk.qsd[ch].q = static_cast<int32_t>(fQ[ch] * 8388607.0);
            }
            batch.push_back(pk); if (batch.size() >= 32) { stream->writeBatch(batch); batch.clear(); }
            outputSample++;
        }
    }
    return 0;
}

} // namespace nexrx

int main(int argc, char* argv[]) {
#ifdef HAVE_SSE_DENORMAL_CONTROL
    _mm_setcsr(_mm_getcsr() | 0x8040);
#endif
    nexrx::Options opts = nexrx::parseArgs(argc, argv);
    if (opts.help) { nexrx::printUsage(argv[0]); return 0; }
    return nexrx::runFunctionalMode(opts);
}
