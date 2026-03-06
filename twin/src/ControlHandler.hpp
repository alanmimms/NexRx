#pragma once

#include "transport/TCPControlTransport.hpp"
#include "AttenuatorModel.hpp"
#include "Control.hpp"

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
    enabled.store(true, std::memory_order_relaxed);
  }

  void setEnabled(bool en) {
    enabled.store(en, std::memory_order_relaxed);
  }

  bool isEnabled() const {
    return enabled.load(std::memory_order_relaxed);
  }

  void setCap(int idx, bool enabledIn) { 
    if (idx >= 0 && idx < 11) {
      caps[idx].store(enabledIn, std::memory_order_relaxed); 
    }
  }

  void setCapMask(uint32_t mask) {
    for (int i = 0; i < 11; ++i) {
      caps[i].store((mask >> i) & 1, std::memory_order_relaxed);
    }
  }

  void setInd(int idx, bool enabled) { 
    if (idx == 0) {
      l1Shorted.store(enabled, std::memory_order_relaxed); 
    }
  }

  void setIndMask(uint32_t mask) {
    l1Shorted.store(mask & 1, std::memory_order_relaxed);
  }

  bool getCap(int idx) const { 
    return (idx >= 0 && idx < 11) ? caps[idx].load(std::memory_order_relaxed) : false; 
  }

  bool isL1Shorted() const {
    return l1Shorted.load(std::memory_order_relaxed);
  }

  uint32_t getCapMask() const {
    uint32_t mask = 0;
    for (int i = 0; i < 11; ++i) {
        if (caps[i].load(std::memory_order_relaxed)) {
            mask |= (1 << i);
        }
    }
    return mask;
  }

  void autoTune(double freqHz) {
    const double pi = 3.141592653589793;
    bool useShort = freqHz > 5.0e6;
    double L = useShort ? 220e-9 : 1720e-9;
    double targetC = 1.0 / (L * std::pow(2.0 * pi * freqHz, 2.0));
    double targetCPF = (targetC * 1e12) - 20.0;
    
    uint16_t mask = 0;
    double currentC = 0;
    static constexpr double capVals[11] = { 8, 15, 33, 68, 120, 250, 560, 1000, 2200, 3900, 8200 };
    for (int i = 10; i >= 0; --i) {
      if (currentC + capVals[i] <= targetCPF + capVals[i] * 0.5) {
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
  std::atomic<bool> enabled;
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
    for (int i = 0; i < 8; ++i) channelMap[i].store(i);
  }
};

class ControlHandler {
public:
  ControlHandler(double f0, double f1, double f2, 
                 AttenuatorModel* atten = nullptr, 
                 PreselectorModel* presel = nullptr, 
                 PGAModel* pga = nullptr);
  ~ControlHandler();

  void start(TCPControlTransport* control, bool verbose);
  void stop();

  double getQSDFreq(int idx) const { 
    return (idx >= 0 && idx < 3) ? qsdFreqHz[idx].load(std::memory_order_relaxed) : 0; 
  }

  double getVFO() const { return vfoHz.load(std::memory_order_relaxed); }
  double getQSDOffset() const { return qsdKHz.load(std::memory_order_relaxed); }

  bool isStreaming() const { return streaming.load(std::memory_order_acquire); }
  bool isConnected() const { return connected.load(std::memory_order_acquire); }
  bool isISGEnabled() const { return isgEnabled.load(std::memory_order_relaxed); }
  double getISGFreq() const { return isgFreqHz.load(std::memory_order_relaxed); }
  bool isTX() const { return trMode.load(std::memory_order_relaxed) == 1; }

  void getCodecConfig(int& rate, std::vector<int>& channelMap) const {
    rate = codec.sampleRate.load(std::memory_order_relaxed);
    channelMap.clear();
    for (int i = 0; i < 8; ++i) channelMap.push_back(codec.channelMap[i].load(std::memory_order_relaxed));
  }

private:
  void run();
  std::vector<uint8_t> handleCborCommand(const std::vector<uint8_t>& request);
  std::vector<uint8_t> encodeResponse(int status, const std::string& payload);

  TCPControlTransport* control_ = nullptr;
  bool verbose_ = false;
  std::atomic<double> vfoHz{14.2e6};
  std::atomic<double> qsdKHz{12000.0};
  std::atomic<double> qsdFreqHz[3];
  AttenuatorModel* attenuator = nullptr;
  PreselectorModel* presel = nullptr;
  PGAModel* pga = nullptr;
  std::atomic<bool> isgEnabled;
  std::atomic<double> isgFreqHz;
  std::atomic<int> agcMode;
  std::atomic<int> trMode{0}; // 0=RX, 1=TX
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
