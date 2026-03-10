// NexRx Digital Twin
//
// Software-in-the-loop signal generator for the NexRx Triple-QSD SDR.
//
// Build target: twin
// Copyright 2026 NexRx Project - MIT License

#include "orchestrator/Orchestrator.hpp"
#include "ControlHandler.hpp"
#include "sampler/ADCSampler.hpp"
#include "sampler/RXControls.hpp"
#include "transport/IQFrame.hpp"
#include "transport/TCPControlTransport.hpp"
#include "transport/UDPStreamTransport.hpp"
#include "stimulus/ToneGenerator.hpp"
#include "stimulus/StimulusLua.hpp"
#include "stimulus/RFCapturePlayer.hpp"
#include "AttenuatorModel.hpp"
#include "AGCManager.hpp"

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
    std::string calFile = "";
    double durationMS = 0.0;
    double rfFreqMHz = 14.120; 
    double loFreqMHz = 14.200;
    double rfAmplitudeMV = 1.0;
    double qsdOffsetKHz = 12.0;
    std::string stimulus = "";
    std::string wavIQ = "";
    double wavFreqMHz = 0.0;
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
	      << "  --wav-iq FILE    Load WAV I/Q file as antenna stimulus\n"
	      << "  --wav-freq MHZ   Center frequency for WAV I/Q stimulus (default: from filename)\n"
	      << "  --swap-iq        Swap I and Q channels for WAV stimulus\n"
	      << "  --headless       Run simulation immediately without waiting for a client\n"
	      << "  --duration MS    Run for specific duration in ms (default: forever)\n"
	      << "  --port PORT      Set TCP control port (default: 5000)\n"
	      << std::endl;
  }

  Options parseArgs(int argc, char* argv[]) {
    Options opts;
    for (int i = 0; i < 3; ++i) {
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
      } else if (arg == "--qsd-offset" && i + 1 < argc) {
	opts.qsdOffsetKHz = std::stod(argv[++i]);
      } else if (arg == "--stimulus" && i + 1 < argc) {
	opts.stimulus = argv[++i];
      } else if (arg == "--wav-iq" && i + 1 < argc) {
	opts.wavIQ = argv[++i];
      } else if (arg == "--wav-freq" && i + 1 < argc) {
	opts.wavFreqMHz = std::stod(argv[++i]);
      } else if (arg == "--swap-iq") {
	opts.swapIQ = true;
      } else if (arg == "--headless") {
	opts.headless = true;
      } else if (arg == "--duration" && i + 1 < argc) {
	opts.durationMS = std::stod(argv[++i]);
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

      std::cout << "[Twin] Simulated Hardware: Initializing with stable random errors" << std::endl;
      std::cout << " Channel | Image Rejection | Gain Error | Phase Error" << std::endl;
      std::cout << "---------+-----------------+------------+-------------" << std::endl;

      for (int i = 0; i < 3; ++i) {
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

  void getPreselParams(const PreselectorModel& model, double& fRes, double& qFactor) {
    static const double capVals[11] = { 
      8e-12, 15e-12, 33e-12, 68e-12, 120e-12, 250e-12, 
      560e-12, 1000e-12, 2200e-12, 3900e-12, 8200e-12 
    };
    double totalC = 20e-12; // Stray
    for (int i = 0; i < 11; ++i) {
      if (model.getCap(i)) {
	totalC += capVals[i];
      }
    }
    double totalL = model.isL1Shorted() ? 220e-9 : (1.5e-6 + 220e-9);
    fRes = 1.0 / (2.0 * M_PI * std::sqrt(totalL * totalC));
    qFactor = 30.0; // Sharp resonance for visible detuning
  }

  double getPreselGain(double freqHz, const PreselectorModel& model) {
    if (!model.isEnabled()) {
      return 1.0;
    }
    double fRes, qFactor;
    getPreselParams(model, fRes, qFactor);
  
    if (std::abs(freqHz) < 1.0) return 0.00001;

    // Standard 2nd order LC bandpass magnitude:
    // |H(jw)| = 1 / sqrt(1 + Q^2 * (f/f0 - f0/f)^2)
    double x = freqHz / fRes;
    double y = fRes / freqHz;
    double gain = 1.0 / std::sqrt(1.0 + std::pow(qFactor * (x - y), 2.0));
  
    // High-frequency leakage floor
    double leak = 0.00001; // -100dB floor
    return std::max(gain, leak);
  }

  int runFunctionalMode(const Options& opts) {
    std::cout << "=== NexRx Digital Twin - Persistent Functional Mode ===" << std::endl;
  
    std::shared_ptr<StimulusManager> stimulusManager;
    std::unique_ptr<sol::state> lua;
    std::string stimulusPath = opts.stimulus.empty() ? "config/stimuli/default.lua" : opts.stimulus;
  
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
      if (player->loadWav(opts.wavIQ)) {
        double freq = opts.wavFreqMHz * 1e6;
        if (freq == 0) {
          // Attempt to extract frequency from filename (SDRuno style: _7150kHz.wav)
          size_t pos = opts.wavIQ.find_last_of("_");
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
        while (!control->acceptClient(std::chrono::milliseconds(100))) {
          // Idle wait
        }
        std::cout << "[Twin] Control client connected from " << control->peerIP() << std::endl;
      }
    
      AttenuatorModel attenuator;
      PreselectorModel preselector;
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
      double k = opts.qsdOffsetKHz * 1000.0;
      auto controlHandler = std::make_unique<ControlHandler>(lo - k, lo + k, lo, &attenuator, &preselector, &pga, &agc);
      if (!opts.headless) {
        controlHandler->start(control.get(), opts.verbose);
      }
    
      double sampleRate = 96000.0;
      double samplePeriod = 1.0 / sampleRate;
      double isgNoiseZi[3][2] = {};
      double isgNoiseZq[3][2] = {};
      double currentVFOs[3] = {0};
      double loPhases[3] = {0, 0, 0};
      double isgPhases[3] = {0, 0, 0};
      auto streamStartTime = std::chrono::steady_clock::now();
      size_t outputSample = 0;
      std::vector<IQFrame> batch;
      batch.reserve(32);
    
      bool streamLogged = false;
      std::cout << "[Twin] Starting session loop" << std::endl;
      bool headlessStreaming = opts.headless;
    
    // =======================================================================
    // AK5578 SHARP ROLL-OFF Digital Filter Emulation with OVERSAMPLING
    // =======================================================================
    struct BiquadCoeffs { double b0, b1, b2, a1, a2; };
    static constexpr int NUM_LPF_STAGES = 3;
    static constexpr int OVERSAMPLE_RATIO = 5;
    static constexpr BiquadCoeffs ak5578_480k_stages[NUM_LPF_STAGES] = {
        {0.0006628600, 0.0008272571, 0.0006628600, -1.5472148716, 0.6157336719},
        {1.0000000000, -0.4832493140, 1.0000000000, -1.5609518734, 0.7359192796},
        {1.0000000000, -0.9740021692, 1.0000000000, -1.6237519754, 0.9064567688},
    };
    double lpf_zi[3][NUM_LPF_STAGES][2] = {};
    double lpf_zq[3][NUM_LPF_STAGES][2] = {};

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

    auto computePhaseInc = [&](double freq_hz, double rate) -> std::pair<double, double> {
        double delta = 2.0 * M_PI * freq_hz / rate;
        return {std::cos(delta), std::sin(delta)};
    };

    double lo_cos[3] = {1,1,1}, lo_sin[3] = {0,0,0};
    double lo_cos_d[3], lo_sin_d[3];

    auto updateLOs = [&](double lo, double k) {
        auto p0 = computePhaseInc(lo - k, 480000.0);
        lo_cos_d[0] = p0.first; lo_sin_d[0] = p0.second;
        auto p1 = computePhaseInc(lo + k, 480000.0);
        lo_cos_d[1] = p1.first; lo_sin_d[1] = p1.second;
        auto p2 = computePhaseInc(lo, 480000.0);
        lo_cos_d[2] = p2.first; lo_sin_d[2] = p2.second;
    };

    double current_lo = opts.loFreqMHz * 1e6;
    double current_k = opts.qsdOffsetKHz * 1000.0;
    updateLOs(current_lo, current_k);

    // --- Set Real-Time Priority ---
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
      if (opts.durationMS > 0 && (outputSample * 1000.0 / 96000.0) >= opts.durationMS) {
          std::cout << "[Twin] Requested duration (" << opts.durationMS << " ms) reached, closing session." << std::endl;
          break;
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
	double fr, q;
	getPreselParams(preselector, fr, q);
	std::cout << "[Twin] Data streaming ACTIVE. Presel resonant at " << (fr/1e6) << " MHz" << std::endl;
	streamLogged = true;
      }

      double baseVFO = controlHandler->getVFO();
      double qsdK = controlHandler->getQSDOffset();
      if (std::abs(baseVFO - current_lo) > 0.1 || std::abs(qsdK - current_k) > 0.1) {
          updateLOs(baseVFO, qsdK);
          current_lo = baseVFO; current_k = qsdK;
      }

      // Pre-calculate chunk-wide gains
      double pg = getPreselGain(current_lo, preselector);
      double attenGain = attenuator.getVoltageGain();
      double pgaGain = std::pow(10.0, pga.getGainDB() / 20.0);

      int nToProcess = 960; // 10ms chunk
      for (int s = 0; s < nToProcess; ++s) {
          double filt_i[3]={0}, filt_q[3]={0};
          for (int i = 0; i < OVERSAMPLE_RATIO; ++i) {
              double t = (outputSample * OVERSAMPLE_RATIO + i) / 480000.0;
              double antI = 0, antQ = 0;
              
              // Calibration Stimulus (Clean sine wave)
              if (controlHandler->isCalStimActive()) {
                  double fStim = controlHandler->getCalStimFreq();
                  double phaseStim = 2.0 * M_PI * std::fmod(fStim * t, 1.0);
                  // Add high-amplitude calibration tone (10mV)
                  antI = 0.010 * std::cos(phaseStim);
                  antQ = 0.010 * std::sin(phaseStim);
              } else {
                  if (stimulusManager) stimulusManager->getRfIQ(t, antI, antQ, current_lo, 100000.0);
              }

              // Apply preselector and attenuator gains
              antI *= (pg * attenGain); 
              antQ *= (pg * attenGain);

              for (int ch = 0; ch < 3; ++ch) {
                  // Complex mixing: S_bb = S_rf * exp(-j*phi_LO)
                  double bb_i = antI * lo_cos[ch] + antQ * lo_sin[ch];
                  double bb_q = antQ * lo_cos[ch] - antI * lo_sin[ch];
                  
                  // Add tiny independent channel noise (Johnson-Nyquist simulation)
                  static thread_local std::mt19937 noiseRng0(54321), noiseRng1(54322), noiseRng2(54323);
                  static thread_local std::normal_distribution<double> noiseDist(0, 1e-10);
                  std::mt19937& rng = (ch == 0) ? noiseRng0 : (ch == 1 ? noiseRng1 : noiseRng2);
                  bb_i += noiseDist(rng);
                  bb_q += noiseDist(rng);

                  // Apply simulated hardware error
                  double gE = opts.gainErr[ch];
                  double pE = opts.phaseErrRad[ch];
                  double err_i = bb_i;
                  double err_q = (bb_q * std::cos(pE) - bb_i * std::sin(pE)) * gE;

                  double fi = applyLpf(err_i, lpf_zi[ch]);
                  double fq = applyLpf(err_q, lpf_zq[ch]);
                  
                  // Proper decimation: The LPF handles anti-aliasing by running at 480kHz.
                  // We take the filtered sample at the decimation point (every 5th sample).
                  if (i == OVERSAMPLE_RATIO - 1) {
                      filt_i[ch] = fi;
                      filt_q[ch] = fq;
                  }

                  // Increment LO phase
                  double c = lo_cos[ch] * lo_cos_d[ch] - lo_sin[ch] * lo_sin_d[ch];
                  double s = lo_sin[ch] * lo_cos_d[ch] + lo_cos[ch] * lo_sin_d[ch];
                  lo_cos[ch] = c; lo_sin[ch] = s;
              }
          }

          // Periodically renormalize LO phases to prevent drift (every 960 samples = 10ms)
          if ((outputSample & 0x3FF) == 0) {
              for (int ch = 0; ch < 3; ++ch) {
                  double m = 1.0 / std::sqrt(lo_cos[ch]*lo_cos[ch] + lo_sin[ch]*lo_sin[ch]);
                  lo_cos[ch] *= m; lo_sin[ch] *= m;
              }
          }

          IQFrame pk;
          pk.sequence = (uint32_t)outputSample;
          pk.timestampNS = static_cast<uint64_t>(outputSample * 1e9 / sampleRate);
          
          static thread_local std::mt19937 rng(12345 + (uint32_t)std::hash<std::thread::id>{}(std::this_thread::get_id()));
          static thread_local std::uniform_real_distribution<double> uniform(-0.5, 0.5);
          constexpr double scale = 8388607.0 / 1.65;

          int32_t maxPeak = 0;
          for (int ch = 0; ch < 3; ++ch) {
              auto quantize = [&](double v) {
                  double dither = uniform(rng) + uniform(rng);
                  return static_cast<int_fast32_t>(std::clamp(std::round(v * pgaGain * scale + dither), -8388608.0, 8388607.0));
              };
              pk.qsd[ch].i = quantize(filt_i[ch]);
              pk.qsd[ch].q = quantize(filt_q[ch]);
              maxPeak = std::max({maxPeak, std::abs(pk.qsd[ch].i), std::abs(pk.qsd[ch].q)});
          }
          
          // Update AGC with peak from this sample
          agc.processReflex(maxPeak);

          batch.push_back(pk);
          outputSample++;

          if (batch.size() >= 32) {
              // --- Smoother Pacing at Packet Level ---
              auto nowP = std::chrono::steady_clock::now();
              double elapsedP = std::chrono::duration<double>(nowP - streamStartTime).count();
              double targetP = static_cast<double>(outputSample) / 96000.0;
              
              if (targetP > elapsedP) {
                  auto waitTime = std::chrono::microseconds(static_cast<int64_t>((targetP - elapsedP) * 1e6));
                  if (waitTime.count() > 100) {
                      std::this_thread::sleep_for(waitTime);
                  }
              } else if (elapsedP - targetP > 0.1) {
                  // Catch-up protection
                  streamStartTime = nowP - std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(targetP));
              }

              if (!opts.headless) {
                  stream->writeBatch(batch);
              }
              batch.clear();
          }
      }
    }
    
      std::cout << "[Twin] Session ended" << std::endl;
      if (!opts.headless) {
        controlHandler->stop();
        stream->disconnect();
        control->closeConnection();
      }

      if (opts.durationMS > 0) {
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
