#pragma once

#include "transport/TCPControlTransport.hpp"
#include "AttenuatorModel.hpp"

#include <cbor.h>
#include <atomic>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <map>

namespace nexrx {

class PreselectorModel {
public:
  PreselectorModel() {
    for (int i = 0; i < 11; ++i) {
      caps[i].store(false, std::memory_order_relaxed);
    }
    l1Shorted.store(false, std::memory_order_relaxed);
  }

  void setCap(int idx, bool enabled) { 
    if (idx >= 0 && idx < 11) {
      caps[idx].store(enabled, std::memory_order_relaxed); 
    }
  }

  void setInd(int idx, bool enabled) { 
    if (idx == 0) {
      l1Shorted.store(enabled, std::memory_order_relaxed); 
    }
  }

  bool getCap(int idx) const { 
    return (idx >= 0 && idx < 11) ? caps[idx].load(std::memory_order_relaxed) : false; 
  }

  bool isL1Shorted() const {
    return l1Shorted.load(std::memory_order_relaxed);
  }

  void autoTune(double freqHz) {
    // Shunt LC: f = 1 / (2*pi*sqrt(L*C))
    // L values: L702=220nH, L701+L702=1720nH
    // C range: ~20pF (stray) to ~16nF
    const double pi = 3.141592653589793;
    
    // Choose inductor based on frequency
    // Threshold ~5MHz: 1.72uH needs 580pF at 5MHz. 220nH needs 4.6nF.
    bool useShort = freqHz > 5.0e6;
    double L = useShort ? 220e-9 : 1720e-9;
    
    // Target C = 1 / (L * (2*pi*f)^2)
    double targetC = 1.0 / (L * std::pow(2.0 * pi * freqHz, 2.0));
    double targetCPF = (targetC * 1e12) - 20.0; // Subtract stray
    
    uint16_t mask = 0;
    double currentC = 0;
    static constexpr double capVals[11] = { 8, 15, 33, 68, 120, 250, 560, 1000, 2200, 3900, 8200 };
    
    // Greedy approximation of capacitor mask
    for (int i = 10; i >= 0; --i) {
      if (currentC + capVals[i] <= targetCPF + capVals[i]/2.0) {
        mask |= (1 << i);
        currentC += capVals[i];
      }
    }
    
    l1Shorted.store(useShort, std::memory_order_relaxed);
    for (int i = 0; i < 11; ++i) {
      caps[i].store((mask >> i) & 1, std::memory_order_relaxed);
    }
  }

private:
  std::atomic<bool> caps[11];
  std::atomic<bool> l1Shorted;
};

class PGAModel {
public:
  PGAModel() {
    gainCode.store(0, std::memory_order_relaxed);
  }

  void setGainCode(int code) {
    gainCode.store(code, std::memory_order_relaxed);
  }

  int getGainCode() const {
    return gainCode.load(std::memory_order_relaxed);
  }

  double getGainDB() const { 
    /* Match MAX9939 simplified linear mapping for now */
    return static_cast<double>(gainCode.load()) * 4.0; 
  }

private:
  std::atomic<int> gainCode;
};

struct CodecConfig {
  std::atomic<int> sampleRate{96000};
  std::atomic<int> channelMap[8];
  std::atomic<double> gain{0.0};
  std::atomic<int> filterType{0};

  CodecConfig() {
    for (int i = 0; i < 8; ++i) {
      channelMap[i].store(i);
    }
  }
};

class ControlHandler {
public:
  static constexpr const char* CMD_SET_VFO         = "SVFO";
  static constexpr const char* CMD_SET_ATTEN       = "SATT";
  static constexpr const char* CMD_SET_PGA_GAIN    = "SPGA";
  static constexpr const char* CMD_SET_AGC_MODE    = "SAGC";
  static constexpr const char* CMD_START_STREAM    = "STM[";
  static constexpr const char* CMD_STOP_STREAM     = "]STM";
  static constexpr const char* CMD_GET_TIMESTAMP   = "GTIM";
  static constexpr const char* CMD_SET_ISG_FREQ    = "SIFQ";
  static constexpr const char* CMD_SET_ISG_ENABLE  = "SIEN";
  static constexpr const char* CMD_SET_PRESEL_IND  = "SPRI";
  static constexpr const char* CMD_SET_PRESEL_CAP  = "SPRC";

  ControlHandler(double f0, double f1, double f2, 
                 AttenuatorModel* atten = nullptr, 
                 PreselectorModel* presel = nullptr, 
                 PGAModel* pga = nullptr);

  ~ControlHandler();

  void start(TCPControlTransport* control, bool verbose);
  void stop();

  double getQSDFreq(int idx) const { 
    if (idx < 0 || idx >= 3) {
      return 0; 
    }
    return qsdFreqHz[idx].load(std::memory_order_relaxed); 
  }

  bool isStreaming() const { return streaming.load(std::memory_order_acquire); }
  bool isConnected() const { return connected.load(std::memory_order_acquire); }
  bool isISGEnabled() const { return isgEnabled.load(std::memory_order_relaxed); }
  double getISGFreq() const { return isgFreqHz.load(std::memory_order_relaxed); }

  void getCodecConfig(int& rate, std::vector<int>& channelMap) const {
    rate = codec.sampleRate.load(std::memory_order_relaxed);
    channelMap.clear();
    for (int i = 0; i < 8; ++i) {
      channelMap.push_back(codec.channelMap[i].load(std::memory_order_relaxed));
    }
  }

private:
  void run();
  std::vector<uint8_t> handleCborCommand(const std::vector<uint8_t>& request);
  std::vector<uint8_t> encodeResponse(int status, const std::string& payload);

  TCPControlTransport* control_ = nullptr;
  bool verbose_ = false;
  std::atomic<double> qsdFreqHz[3];
  AttenuatorModel* attenuator = nullptr;
  PreselectorModel* presel = nullptr;
  PGAModel* pga = nullptr;
  std::atomic<bool> isgEnabled;
  std::atomic<double> isgFreqHz;
  std::atomic<int> agcMode;
  CodecConfig codec;
  std::map<std::string, std::string> calibrations;
  std::atomic<bool> streaming;
  std::atomic<bool> running;
  std::atomic<bool> connected;
  std::atomic<bool> reconnected;
  std::string newClientIP;
  std::mutex reconnectMutex;
  std::thread thread_;
};

} // namespace nexrx
