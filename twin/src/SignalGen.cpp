// NexRx Digital Twin
//
// Software-in-the-loop signal generator for the NexRx Dual-OSD SDR.
//
// Build target: twin
// Copyright 2026 NexRx Project - MIT License

#include "orchestrator/Orchestrator.hpp"
#include "ControlHandler.hpp"
#include "sampler/ADCSampler.hpp"
#include "sampler/RXControls.hpp"
#include "transport/IQFrame.hpp"
#include "transport/IQPacketHeader.hpp"
#include "transport/TCPControlTransport.hpp"
#include "transport/UDPStreamTransport.hpp"
#include "stimulus/ToneGenerator.hpp"
#include "stimulus/StimulusLua.hpp"
#include "stimulus/RFCapturePlayer.hpp"
#include "stimulus/AMGenerator.hpp"
#include "AttenuatorModel.hpp"
#include "AGCManager.hpp"
#include "CPLDModel.hpp"

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
    bool headless = false;
    bool swapIQ = false;
    bool noStimulus = false;
    bool noCal = false;
    bool singleSession = false;
    double timeoutSec = 0.0;
    std::string calFile = "";
    double durationMS = 0.0;
    double rfFreqMHz = 14.120; 
    double loFreqMHz = 14.200;
    double rfAmplitudeMV = 1.0;
    double osdOffsetKHz = 12.0;
    std::string stimulus = "";
    std::string wavIQ = "";
    double wavFreqMHz = 0.0;
    std::string bindAddr = "0.0.0.0";
    uint16_t controlPort = 5000;
    uint16_t streamPort = 5001;

    std::string dumpStage3InFile = "";
    std::string dumpStage3OutFile = "";
    size_t dumpLimit = 0;

    double gainErr[2];
    double phaseErrRad[2];
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
	      << "  --osd-offset KHZ Set OSD offset k in kHz (default: 12.0)\n"
	      << "  --stimulus FILE  Load Lua stimulus script (overrides --rf)\n"
	      << "  --wav-iq FILE    Load WAV I/Q file as antenna stimulus\n"
	      << "  --wav-freq MHZ   Center frequency for WAV I/Q stimulus (default: from filename)\n"
	      << "  --swap-iq        Swap I and Q channels for WAV stimulus\n"
	      << "  --dump-stage3-in FILE  Dump Stage 3 input (pre-recomb 4x float64) to FILE\n"
	      << "  --dump-stage3-out FILE Dump Stage 3 output (post-recomb 2x float64) to FILE\n"
	      << "  --dump-limit N         Limit Stage 3 dump to N samples (default: 0 = unlimited)\n"
	      << "  --headless       Run simulation immediately without waiting for a client\n"
	      << "  --duration MS    Run for specific duration in ms (default: forever)\n"
	      << "  --timeout SEC    Exit after SEC seconds total or idle waiting (default: forever)\n"
	      << "  --single-session Exit after first client session ends\n"
	      << "  --port PORT      Set TCP control port (default: 5000)\n"
	      << std::endl;
  }

  Options parseArgs(int argc, char* argv[]) {
    Options opts;
    for (int i = 0; i < 2; ++i) {
      opts.gainErr[i] = 1.0;
      opts.phaseErrRad[i] = 0.0;
    }

    for (int i = 1; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg == "--help" || arg == "-h") {
	opts.help = true;
      } else if (arg == "--quiet") {
	opts.verbose = false;
      } else if (arg == "--no-stimulus") {
	opts.noStimulus = true;
      } else if (arg == "--no-cal") {
	opts.noCal = true;
      } else if (arg == "--cal-file" && i + 1 < argc) {
	opts.calFile = argv[++i];
      } else if (arg == "--rf" && i + 1 < argc) {
	opts.rfFreqMHz = std::stod(argv[++i]);
      } else if (arg == "--lo" && i + 1 < argc) {
	opts.loFreqMHz = std::stod(argv[++i]);
      } else if (arg == "--amplitude" && i + 1 < argc) {
	opts.rfAmplitudeMV = std::stod(argv[++i]);
      } else if ((arg == "--osd-offset" || arg == "--qsd-offset") && i + 1 < argc) {
	opts.osdOffsetKHz = std::stod(argv[++i]);
      } else if (arg == "--stimulus" && i + 1 < argc) {
	opts.stimulus = argv[++i];
      } else if (arg == "--wav-iq" && i + 1 < argc) {
	opts.wavIQ = argv[++i];
      } else if (arg == "--wav-freq" && i + 1 < argc) {
	opts.wavFreqMHz = std::stod(argv[++i]);
      } else if (arg == "--swap-iq") {
	opts.swapIQ = true;
      } else if (arg == "--dump-stage3-in" && i + 1 < argc) {
	opts.dumpStage3InFile = argv[++i];
      } else if (arg == "--dump-stage3-out" && i + 1 < argc) {
	opts.dumpStage3OutFile = argv[++i];
      } else if (arg == "--dump-limit" && i + 1 < argc) {
	opts.dumpLimit = std::stoull(argv[++i]);
      } else if (arg == "--headless") {
	opts.headless = true;
      } else if (arg == "--duration" && i + 1 < argc) {
	opts.durationMS = std::stod(argv[++i]);
      } else if (arg == "--timeout" && i + 1 < argc) {
	opts.timeoutSec = std::stod(argv[++i]);
      } else if (arg == "--single-session") {
	opts.singleSession = true;
      } else if (arg == "--port" && i + 1 < argc) {
	opts.controlPort = (uint16_t)std::stoi(argv[++i]);
      } else {
	std::cerr << "Unknown option: " << arg << std::endl;
	printUsage(argv[0]);
	exit(1);
      }
    }

    if (opts.calFile.empty() && !opts.noCal) {
      std::mt19937 rng(1337); // Stable seed for simulation consistency
      std::uniform_real_distribution<double> gDist(0.95, 1.05);
      std::uniform_real_distribution<double> pDist(-0.05, 0.05);

      std::cout << "[Twin] Simulated Hardware: Initializing with stable random errors (Dual OSD)" << std::endl;
      std::cout << " Channel | Image Rejection | Gain Error | Phase Error" << std::endl;
      std::cout << "---------+-----------------+------------+-------------" << std::endl;

      for (int i = 0; i < 2; ++i) {
	double g = gDist(rng);
	double p = pDist(rng);
	opts.gainErr[i] = g;
	opts.phaseErrRad[i] = p;
	
	// IR = (1 + G^2 + 2G cos P) / (1 + G^2 - 2G cos P)
	double num = 1.0 + g*g + 2.0*g*std::cos(p);
	double den = 1.0 + g*g - 2.0*g*std::cos(p);
	double rej = 10.0 * std::log10(num / std::max(1e-10, den));
	std::cout << std::setw(7) << i << " | " << std::fixed << std::setprecision(1) << std::setw(7) << rej << " dBc | " << std::setw(15) << std::setprecision(3) << 20.0*std::log10(g) << " dB | " << std::setw(15) << std::setprecision(2) << p * (180.0/M_PI) << " deg" << std::endl;
      }
      std::cout << std::endl;
    } else if (opts.noCal) {
      std::cout << "[Twin] Simulated Hardware: Using ideal components (--no-cal specified)" << std::endl;
    }
    return opts;
  }

  double getChebyshevGain(double x, int n, double rippleDB) {
    double epsilon = std::sqrt(std::pow(10.0, rippleDB / 10.0) - 1.0);
    double cn;
    double ax = std::abs(x);
    if (ax <= 1.0) {
        cn = std::cos(n * std::acos(ax));
    } else {
        cn = std::cosh(n * std::acosh(ax));
    }
    return 1.0 / std::sqrt(1.0 + epsilon * epsilon * cn * cn);
  }

  double getFilterBankGain(double freqHz, const FilterBankModel& filters) {
    double totalGain = 1.0;
    
    // 1. AM Reject HPF (5th order Chebyshev, 1.75 MHz cutoff, 0.5dB ripple)
    if (!filters.isHpfBypassed()) {
        double x = 1.75e6 / std::max(1.0, freqHz);
        totalGain *= getChebyshevGain(x, 5, 0.5);
    }
    
    // 2. BPF Tree (3rd order Chebyshev, 0.5dB ripple)
    int bpfIdx = filters.getBpfIndex();
    if (bpfIdx >= 1 && bpfIdx <= 5) {
        double fStart, fEnd;
        switch (bpfIdx) {
            case 1: fStart = 1.8e6;  fEnd = 3.4e6;  break;
            case 2: fStart = 3.2e6;  fEnd = 7.5e6;  break;
            case 3: fStart = 7.3e6;  fEnd = 14.5e6; break;
            case 4: fStart = 14.3e6; fEnd = 22.0e6; break;
            case 5: fStart = 21.8e6; fEnd = 30.0e6; break;
            default: return totalGain;
        }
        double bw = fEnd - fStart;
        double f0 = std::sqrt(fStart * fEnd);
        double x = (freqHz * freqHz - f0 * f0) / (freqHz * bw);
        totalGain *= getChebyshevGain(x, 3, 0.5);
    }
    
    return std::max(totalGain, 0.00001);
  }

  int runFunctionalMode(const Options& opts) {
    std::cout << "=== NexRx Digital Twin - Persistent Functional Mode ===" << std::endl;
    auto programStartTime = std::chrono::steady_clock::now();
  
    std::shared_ptr<StimulusManager> stimulusManager;
    std::unique_ptr<sol::state> lua;
    std::string stimulusPath = opts.stimulus;
    if (stimulusPath.empty()) {
      if (std::ifstream("config/stimuli/default.lua").good()) {
        stimulusPath = "config/stimuli/default.lua";
      } else if (std::ifstream("twin/config/stimuli/default.lua").good()) {
        stimulusPath = "twin/config/stimuli/default.lua";
      } else {
        stimulusPath = "config/stimuli/default.lua";
      }
    } else if (!std::ifstream(stimulusPath).good() && std::ifstream("twin/" + stimulusPath).good()) {
      stimulusPath = "twin/" + stimulusPath;
    }
  
    if (!opts.noStimulus && std::ifstream(stimulusPath).good()) {
      lua = std::make_unique<sol::state>();
      lua->open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);
      StimulusLua stimLua;
      stimLua.registerBindings(*lua);
      if (stimLua.loadScript(*lua, stimulusPath)) {
	stimulusManager = stimLua.manager();
	stimulusManager->freeze();
	if (opts.verbose) {
	  std::cout << "[Twin] Loaded stimulus: " << stimulusPath << std::endl;
	}
      }
    }

    if (!opts.wavIQ.empty()) {
      if (!stimulusManager) {
        stimulusManager = std::make_shared<StimulusManager>();
      }
      auto player = std::make_shared<RFCapturePlayer>();
      std::string wavPath = opts.wavIQ;
      if (!std::ifstream(wavPath).good() && std::ifstream("twin/" + opts.wavIQ).good()) {
        wavPath = "twin/" + opts.wavIQ;
      }
      if (player->loadWav(wavPath)) {
        double freq = opts.wavFreqMHz * 1e6;
        if (freq == 0) {
          // Attempt to extract frequency from filename (SDRuno style: _7150kHz.wav)
          size_t pos = wavPath.find_last_of("_");
          if (pos != std::string::npos) {
            try {
              // Extract numeric part before "kHz"
              std::string sub = opts.wavIQ.substr(pos + 1);
              size_t kPos = sub.find("kHz");
              if (kPos != std::string::npos) {
                  freq = std::stod(sub.substr(0, kPos)) * 1000.0;
              }
            } catch (...) {}
          }
        }
        player->setCenterFrequency(freq);
        player->setLooping(true);
        player->setSwapIQ(opts.swapIQ);
        stimulusManager->addStimulus("wav-iq", player, "rf-capture", freq, 1.0);
        stimulusManager->freeze();
        std::cout << "[Twin] Loaded WAV I/Q stimulus: " << opts.wavIQ << " at " << freq/1e6 << " MHz" << std::endl;
      } else {
        std::cerr << "[Twin] FAILED to load WAV I/Q: " << opts.wavIQ << std::endl;
      }
    }

    // Add 1.5 MHz AM broadcast source for HPF testing
    if (stimulusManager) {
        auto amSource = std::make_shared<AMGenerator>(1.5e6, 0.05); // 1.5 MHz, 50mV peak
        amSource->setTones({1000.0}); // 1 kHz tone
        amSource->setModulationIndex(0.8);
        stimulusManager->addStimulus("am-broadcast", amSource, "am", 1.5e6, 5.0); // Strong signal
        stimulusManager->freeze();
    }
  
    TCPControlConfig ctlConfig; 
    ctlConfig.host = opts.bindAddr; 
    ctlConfig.port = opts.controlPort; 
    ctlConfig.server = true;
    auto control = std::make_unique<TCPControlTransport>(ctlConfig);
    if (!opts.headless) {
      if (!control->connect()) {
        std::cerr << "[Twin] Failed to bind to control port " << opts.controlPort << std::endl;
        return 1; 
      }
    }
  
    while (true) {
      if (!opts.headless) {
        std::cout << "[Twin] Waiting for control connection on port " << opts.controlPort << "..." << std::endl;
        auto waitStart = std::chrono::steady_clock::now();
        while (!control->acceptClient(std::chrono::milliseconds(100))) {
          if (opts.timeoutSec > 0.0) {
            double waitElapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - waitStart).count();
            if (waitElapsed >= opts.timeoutSec) {
              std::cout << "[Twin] Timeout waiting for client connection (" << opts.timeoutSec << " s reached)." << std::endl;
              return 0;
            }
          }
        }
        std::cout << "[Twin] Control client connected from " << control->peerIP() << std::endl;
      }
    
      AttenuatorModel attenuator;
      FilterBankModel filters;
      PGAModel pga;
      AGCManager agc(&attenuator, &pga);
    
      // Default PGA to healthy 20dB gain for testing
      pga.setGainCode(5); 

      UDPStreamConfig streamConfig; 
      streamConfig.host = control->peerIP(); 
      streamConfig.port = opts.streamPort; 
      streamConfig.server = true;
      auto stream = std::make_unique<UDPStreamTransport>(streamConfig);
      if (!stream->connect()) {
	std::cerr << "[Twin] Failed to initialize UDP stream" << std::endl;
	control->closeConnection();
	continue;
      }
      stream->setDestination(control->peerIP(), opts.streamPort);
      std::cout << "[Twin] Streaming IQ data to " << control->peerIP() << ":" << opts.streamPort << std::endl;
    
      double lo = opts.loFreqMHz * 1e6;
      double k = opts.osdOffsetKHz * 1000.0;
      CPLDModel cpldModel;
      auto controlHandler = std::make_unique<ControlHandler>(lo, k, &attenuator, &filters, &pga, &agc);
      if (!opts.headless) {
        controlHandler->start(control.get(), opts.verbose);
      }
    
      double sampleRate = 384000.0;
      double samplePeriod = 1.0 / sampleRate;
      auto streamStartTime = std::chrono::steady_clock::now();
      size_t outputSample = 0;
      std::vector<IQFrame> batch;
      batch.reserve(32);
    
      std::ofstream dumpStage3InStream;
      if (!opts.dumpStage3InFile.empty()) {
        dumpStage3InStream.open(opts.dumpStage3InFile, std::ios::binary);
        if (dumpStage3InStream.is_open()) {
          std::cout << "[Twin] Dumping Stage 3 input (pre-recomb 4x float64) to: " << opts.dumpStage3InFile << std::endl;
        } else {
          std::cerr << "[Twin] WARNING: Failed to open Stage 3 input dump file: " << opts.dumpStage3InFile << std::endl;
        }
      }

      std::ofstream dumpStage3OutStream;
      if (!opts.dumpStage3OutFile.empty()) {
        dumpStage3OutStream.open(opts.dumpStage3OutFile, std::ios::binary);
        if (dumpStage3OutStream.is_open()) {
          std::cout << "[Twin] Dumping Stage 3 output (post-recomb 2x float64) to: " << opts.dumpStage3OutFile << std::endl;
        } else {
          std::cerr << "[Twin] WARNING: Failed to open Stage 3 output dump file: " << opts.dumpStage3OutFile << std::endl;
        }
      }

      size_t samplesDumpedIn = 0;
      size_t samplesDumpedOut = 0;

      uint64_t totalRFSamples = 0;
      uint64_t totalBasebandSamples = 0;
      uint64_t totalPacketsSent = 0;
      uint64_t totalBytesSent = 0;
      uint64_t catchUpCount = 0;

      auto lastStatTime = std::chrono::steady_clock::now();
      uint64_t lastRFSamples = 0;
      uint64_t lastBasebandSamples = 0;
      uint64_t lastPacketsSent = 0;
      uint64_t lastBytesSent = 0;
      uint64_t lastCatchUpCount = 0;
      double intervalComputeTimeS = 0.0;

      bool streamLogged = false;
      std::cout << "[Twin] Starting session loop (Dual OSD)" << std::endl;
      bool headlessStreaming = opts.headless;
    
    // =======================================================================
    // TLV320ADC5140 Digital Filter Emulation with OVERSAMPLING
    // =======================================================================
    struct BiquadCoeffs { double b0, b1, b2, a1, a2; };
    static constexpr int NUM_LPF_STAGES = 3;
    static constexpr int OVERSAMPLE_RATIO = 5;
    const double simSampleRate = sampleRate * OVERSAMPLE_RATIO;
    static constexpr BiquadCoeffs tlv320_stages[NUM_LPF_STAGES] = {
        {0.0006628600, 0.0008272571, 0.0006628600, -1.5472148716, 0.6157336719},
        {1.0000000000, -0.4832493140, 1.0000000000, -1.5609518734, 0.7359192796},
        {1.0000000000, -0.9740021692, 1.0000000000, -1.6237519754, 0.9064567688},
    };
    double lpf_zi[2][NUM_LPF_STAGES][2] = {};
    double lpf_zq[2][NUM_LPF_STAGES][2] = {};

    // Fast PRNG for noise generation (Xorwow)
    struct FastNoise {
        uint32_t x=123456789, y=362436069, z=521288629, w=88675123, v=5783321, d=6615241;
        void seed(uint32_t s) { x = s; y = s*2; z = s*3; w = s*4; v = s*5; d = s*6; }
        double next() {
            uint32_t t = (x ^ (x >> 2));
            x = y; y = z; z = w; w = v;
            v = (v ^ (v << 4)) ^ (t ^ (t << 1));
            return (static_cast<double>((v + (d += 362437)) & 0xFFFFFF) / 16777216.0) - 0.5;
        }
    } noiseGens[2];
    for (int i=0; i<2; ++i) noiseGens[i].seed(1337 + i);

    auto applyLpf = [&](double x, double z[NUM_LPF_STAGES][2]) -> double {
        double y = x;
        for (int s = 0; s < NUM_LPF_STAGES; ++s) {
            const auto& c = tlv320_stages[s];
            double out = c.b0 * y + z[s][0];
            z[s][0] = c.b1 * y - c.a1 * out + z[s][1];
            z[s][1] = c.b2 * y - c.a2 * out;
            y = out;
        }
        return y;
    };

    auto computePhaseInc = [&](double freq_hz, double rate) -> std::pair<double, double> {
        double delta = 2.0 * M_PI * freq_hz / rate;
        return {std::cos(delta), std::sin(delta)};
    };

    double lo_cos[2] = {1,1}, lo_sin[2] = {0,0};
    double lo_cos_d[2], lo_sin_d[2];

    auto updateLOs = [&](double lo, double k) {
        auto p0 = computePhaseInc(lo - k, simSampleRate);
        lo_cos_d[0] = p0.first; lo_sin_d[0] = p0.second;
        auto p1 = computePhaseInc(lo + k, simSampleRate);
        lo_cos_d[1] = p1.first; lo_sin_d[1] = p1.second;
    };

    double current_lo = opts.loFreqMHz * 1e6;
    double current_k = opts.osdOffsetKHz * 1000.0;
    updateLOs(current_lo, current_k);

    double recombCos = 1.0, recombSin = 0.0;
    double recombCos_d = 1.0, recombSin_d = 0.0;
    auto updateRecomb = [&](double k) {
      double delta = 2.0 * M_PI * k / sampleRate;
      recombCos_d = std::cos(delta);
      recombSin_d = std::sin(delta);
    };
    updateRecomb(current_k);

    std::vector<double> antBufferIQ(3840 * OVERSAMPLE_RATIO * 2);

    #ifndef _WIN32
    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_RR);
    if (pthread_setschedparam(pthread_self(), SCHED_RR, &param) != 0) {
        std::cerr << "[Twin] WARNING: Failed to set real-time priority. Run with sudo for better performance." << std::endl;
    } else {
        std::cout << "[Twin] Real-time priority (SCHED_RR) enabled." << std::endl;
    }
    #endif

    while (opts.headless || (controlHandler && controlHandler->isConnected())) {
      if (opts.durationMS > 0 && (outputSample * 1000.0 / sampleRate) >= opts.durationMS) {
        std::cout << "[Twin] Requested duration (" << opts.durationMS << " ms) reached, closing session." << std::endl;
        break;
      }

      if (opts.timeoutSec > 0.0) {
        double totalElapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - programStartTime).count();
        if (totalElapsed >= opts.timeoutSec) {
          std::cout << "[Twin] Timeout (" << opts.timeoutSec << " s) reached, closing session." << std::endl;
          break;
        }
      }

      if (!headlessStreaming && !controlHandler->isStreaming()) {
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	streamStartTime = std::chrono::steady_clock::now();
	outputSample = 0;
	streamLogged = false;
	if (stimulusManager) stimulusManager->resetAll();
	continue;
      }
      
      if (!streamLogged) {
	std::cout << "[Twin] Data streaming ACTIVE." << std::endl;
	streamLogged = true;
      }

      double baseVFO = controlHandler->getVFO();
      double osdK = controlHandler->getOSDOffset() * 1000.0;
      if (std::abs(baseVFO - current_lo) > 0.1 || std::abs(osdK - current_k) > 0.1) {
        updateLOs(baseVFO, osdK);
        updateRecomb(osdK);
        current_lo = baseVFO; current_k = osdK;
        constexpr double streamSampleRateHz = 384000.0; // 384 k samples/sec
        constexpr double halfSpanHz = streamSampleRateHz / 2.0;
        cpldModel.updateWaterfallViewport(current_lo - halfSpanHz, current_lo + halfSpanHz);
      }

      double attenGain = attenuator.getVoltageGain();
      double pgaGain = std::pow(10.0, pga.getGainDB() / 20.0);
      double modeGainScale = cpldModel.getModeGainScale();

      int nToProcess = 3840; // 10ms chunk
      double chunkStartTime = (outputSample * OVERSAMPLE_RATIO) / simSampleRate;
      double oversamplePeriod = 1.0 / simSampleRate;
      
      auto batchComputeStart = std::chrono::steady_clock::now();
      if (stimulusManager) {
          auto gainFunc = [&](double f) {
              return getFilterBankGain(f, filters);
          };
          stimulusManager->generateBatch(chunkStartTime, oversamplePeriod, nToProcess * OVERSAMPLE_RATIO, 
                                        antBufferIQ.data(), current_lo, simSampleRate, gainFunc);
      } else {
          std::fill(antBufferIQ.begin(), antBufferIQ.end(), 0.0);
      }

      for (int s = 0; s < nToProcess; ++s) {
          double filt_i[2]={0}, filt_q[2]={0};
          for (int i = 0; i < OVERSAMPLE_RATIO; ++i) {
              int bufIdx = (s * OVERSAMPLE_RATIO + i) * 2;
              double antI = antBufferIQ[bufIdx];
              double antQ = antBufferIQ[bufIdx + 1];
              double t = chunkStartTime + (s * OVERSAMPLE_RATIO + i) * oversamplePeriod;
              
              if (controlHandler->isCalStimActive() || controlHandler->isISGEnabled()) {
                double fStim = controlHandler->isISGEnabled() ? controlHandler->getISGFreq() : controlHandler->getCalStimFreq();
                double pgStim = getFilterBankGain(fStim, filters);
                double phaseStim = 2.0 * M_PI * std::fmod(fStim * t, 1.0);
                antI = 0.010 * std::cos(phaseStim) * pgStim;
                antQ = 0.010 * std::sin(phaseStim) * pgStim;
              }

              antI *= attenGain * modeGainScale; 
              antQ *= attenGain * modeGainScale;

              for (int ch = 0; ch < 2; ++ch) {
                  double bb_i = antI * lo_cos[ch] + antQ * lo_sin[ch];
                  double bb_q = antQ * lo_cos[ch] - antI * lo_sin[ch];
                  
                  double cos2 = lo_cos[ch]*lo_cos[ch] - lo_sin[ch]*lo_sin[ch];
                  double sin2 = 2.0*lo_cos[ch]*lo_sin[ch];
                  bb_i += 0.001 * (antI * cos2 + antQ * sin2); 
                  bb_q += 0.001 * (antQ * cos2 - antI * sin2);

                  double cos3 = lo_cos[ch]*cos2 - lo_sin[ch]*sin2;
                  double sin3 = lo_sin[ch]*cos2 + lo_cos[ch]*sin2;
                  bb_i += 0.33 * (antI * cos3 + antQ * sin3);
                  bb_q += 0.33 * (antQ * cos3 - antI * sin3);
                  
                  bb_i += noiseGens[ch].next() * 2e-11; 
                  bb_q += noiseGens[ch].next() * 2e-11;

                  double gE = opts.gainErr[ch];
                  double pE = opts.phaseErrRad[ch];
                  double err_i = bb_i;
                  double err_q = (bb_q * std::cos(pE) - bb_i * std::sin(pE)) * gE;

                  double fi = applyLpf(err_i, lpf_zi[ch]);
                  double fq = applyLpf(err_q, lpf_zq[ch]);
                  
                  if (i == OVERSAMPLE_RATIO - 1) {
                      filt_i[ch] = fi;
                      filt_q[ch] = fq;
                  }

                  double c = lo_cos[ch] * lo_cos_d[ch] - lo_sin[ch] * lo_sin_d[ch];
                  double s = lo_sin[ch] * lo_cos_d[ch] + lo_cos[ch] * lo_sin_d[ch];
                  lo_cos[ch] = c; lo_sin[ch] = s;
              }
          }

          if ((outputSample % 3840) == 0) {
              for (int ch = 0; ch < 2; ++ch) {
                  double m = 1.0 / std::sqrt(lo_cos[ch]*lo_cos[ch] + lo_sin[ch]*lo_sin[ch]);
                  lo_cos[ch] *= m; lo_sin[ch] *= m;
              }
              double rm = 1.0 / std::sqrt(recombCos * recombCos + recombSin * recombSin);
              recombCos *= rm; recombSin *= rm;
          }

          // Dump Stage 3 input (pre-recombination 4 channels: OSD0 I/Q, OSD1 I/Q)
          if (dumpStage3InStream.is_open() && (opts.dumpLimit == 0 || samplesDumpedIn < opts.dumpLimit)) {
            double buf[4] = { filt_i[0], filt_q[0], filt_i[1], filt_q[1] };
            dumpStage3InStream.write(reinterpret_cast<const char*>(buf), sizeof(buf));
            samplesDumpedIn++;
            if (opts.dumpLimit > 0 && samplesDumpedIn == opts.dumpLimit) {
              std::cout << "[Twin] Reached Stage 3 input dump limit (" << opts.dumpLimit << " samples)" << std::endl;
              dumpStage3InStream.flush();
            }
          }

          // STM32 on-chip recombination:
          // Shift OSD0 DOWN by k: (i + j*q) * (cos - j*sin)
          double s0_s_i = filt_i[0] * recombCos + filt_q[0] * recombSin;
          double s0_s_q = filt_q[0] * recombCos - filt_i[0] * recombSin;

          // Shift OSD1 UP by k: (i + j*q) * (cos + j*sin)
          double s1_s_i = filt_i[1] * recombCos - filt_q[1] * recombSin;
          double s1_s_q = filt_q[1] * recombCos + filt_i[1] * recombSin;

          // Coherently sum both OSDs into a single orthogonal I/Q pair
          double comb_i = (s0_s_i + s1_s_i) * 0.5;
          double comb_q = (s0_s_q + s1_s_q) * 0.5;

          // Dump Stage 3 output (post-recombination 2 channels: Combined I/Q)
          if (dumpStage3OutStream.is_open() && (opts.dumpLimit == 0 || samplesDumpedOut < opts.dumpLimit)) {
            double buf[2] = { comb_i, comb_q };
            dumpStage3OutStream.write(reinterpret_cast<const char*>(buf), sizeof(buf));
            samplesDumpedOut++;
            if (opts.dumpLimit > 0 && samplesDumpedOut == opts.dumpLimit) {
              std::cout << "[Twin] Reached Stage 3 output dump limit (" << opts.dumpLimit << " samples)" << std::endl;
              dumpStage3OutStream.flush();
            }
          }

          // Advance recombination phasor
          double nextRCos = recombCos * recombCos_d - recombSin * recombSin_d;
          double nextRSin = recombSin * recombCos_d + recombCos * recombSin_d;
          recombCos = nextRCos;
          recombSin = nextRSin;

          IQFrame pk;
          pk.sequence = (uint32_t)outputSample;
          pk.timestampNS = static_cast<uint64_t>(outputSample * 1e9 / sampleRate);
          
          constexpr double scale = 8388607.0 / 1.65;
          auto quantize = [&](double v) {
              double dither = noiseGens[0].next() * 2.0;
              return static_cast<int_fast32_t>(std::clamp(std::round(v * pgaGain * scale + dither), -8388608.0, 8388607.0));
          };

          pk.sample.i = quantize(comb_i);
          pk.sample.q = quantize(comb_q);
          int32_t maxPeak = std::max(std::abs(pk.sample.i), std::abs(pk.sample.q));
          
          // Update AGC with peak from this sample
          agc.processReflex(maxPeak);

          batch.push_back(pk);
          outputSample++;

          if (batch.size() >= 32) {
              // --- Smoother Pacing at Packet Level ---
              auto nowP = std::chrono::steady_clock::now();
              double elapsedP = std::chrono::duration<double>(nowP - streamStartTime).count();
              double targetP = static_cast<double>(outputSample) / sampleRate;
              
              if (targetP > elapsedP) {
                  auto waitTime = std::chrono::microseconds(static_cast<int64_t>((targetP - elapsedP) * 1e6));
                  if (waitTime.count() > 100) {
                      std::this_thread::sleep_for(waitTime);
                  }
              } else if (elapsedP - targetP > 0.1) {
                  // Catch-up protection
                  catchUpCount++;
                  streamStartTime = nowP - std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(targetP));
              }

              if (!opts.headless) {
                  stream->writeBatch(batch);
              }
              totalPacketsSent++;
              totalBytesSent += sizeof(IQPacketHeader) + batch.size() * sizeof(int32_t) * 2;
              batch.clear();
          }
      }

      auto batchComputeEnd = std::chrono::steady_clock::now();
      intervalComputeTimeS += std::chrono::duration<double>(batchComputeEnd - batchComputeStart).count();
      totalRFSamples += static_cast<uint64_t>(nToProcess) * OVERSAMPLE_RATIO;
      totalBasebandSamples += nToProcess;

      auto nowStat = std::chrono::steady_clock::now();
      double statElapsed = std::chrono::duration<double>(nowStat - lastStatTime).count();
      if (statElapsed >= 1.0) {
        double rfRateMsps = static_cast<double>(totalRFSamples - lastRFSamples) / (statElapsed * 1e6);
        double bbRateKsps = static_cast<double>(totalBasebandSamples - lastBasebandSamples) / (statElapsed * 1e3);
        double pktRate = static_cast<double>(totalPacketsSent - lastPacketsSent) / statElapsed;
        double mbRate = static_cast<double>(totalBytesSent - lastBytesSent) / (statElapsed * 1e6);
        uint64_t catchUpDelta = catchUpCount - lastCatchUpCount;
        double cpuPercent = (intervalComputeTimeS / statElapsed) * 100.0;

        std::cout << "[Twin Stats 1s] Stage A: RF " << std::fixed << std::setprecision(2) << rfRateMsps 
                  << " Msps, Baseband " << std::setprecision(1) << bbRateKsps 
                  << " ksps | Stage B: Net " << std::setprecision(0) << pktRate 
                  << " pkts/s (" << std::setprecision(2) << mbRate << " MB/s) | Compute CPU: " 
                  << std::setprecision(1) << cpuPercent << "% | Deadlines Missed: " 
                  << catchUpDelta << " (total " << catchUpCount << ")" << std::endl;

        lastStatTime = nowStat;
        lastRFSamples = totalRFSamples;
        lastBasebandSamples = totalBasebandSamples;
        lastPacketsSent = totalPacketsSent;
        lastBytesSent = totalBytesSent;
        lastCatchUpCount = catchUpCount;
        intervalComputeTimeS = 0.0;
      }
    }
    
      std::cout << "[Twin] Session ended" << std::endl;
      if (dumpStage3InStream.is_open()) dumpStage3InStream.close();
      if (dumpStage3OutStream.is_open()) dumpStage3OutStream.close();
      if (!opts.headless) {
        controlHandler->stop();
        stream->disconnect();
        control->closeConnection();
      }

      if (opts.durationMS > 0 || opts.singleSession || opts.timeoutSec > 0.0) {
        break;
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
  if (opts.help) {
    nexrx::printUsage(argv[0]);
    return 0;
  }
  return nexrx::runFunctionalMode(opts);
}
