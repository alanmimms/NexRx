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

class AGCManager;

class FilterBankModel {
public:
  FilterBankModel() {
    hpfBypass.store(false, std::memory_order_relaxed);
    bpfIndex.store(0, std::memory_order_relaxed);
  }

  void setHpfBypass(bool bypass) {
    hpfBypass.store(bypass, std::memory_order_relaxed);
  }

  bool isHpfBypassed() const {
    return hpfBypass.load(std::memory_order_relaxed);
  }

  void setBpfIndex(int index) {
    bpfIndex.store(index, std::memory_order_relaxed);
  }

  int getBpfIndex() const {
    return bpfIndex.load(std::memory_order_relaxed);
  }

private:
  std::atomic<bool> hpfBypass;
  std::atomic<int> bpfIndex; // 0=None/Bypass, 1=1.8-3.4, 2=3.2-7.5, 3=7.3-14.5, 4=14.3-22, 5=21.8-30
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
                 FilterBankModel* filters = nullptr, 
                 PGAModel* pga = nullptr,
                 AGCManager* agc = nullptr);
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
  bool isCalStimActive() const { return calStimEnabled.load(std::memory_order_relaxed); }
  double getCalStimFreq() const { return calStimFreqHz.load(std::memory_order_relaxed); }

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
  FilterBankModel* filters = nullptr;
  PGAModel* pga = nullptr;
  AGCManager* agc = nullptr;
  std::atomic<bool> isgEnabled;
  std::atomic<double> isgFreqHz;
  std::atomic<int> agcMode;
  std::atomic<int> trMode{0}; // 0=RX, 1=TX
  std::atomic<bool> calStimEnabled{false};
  std::atomic<double> calStimFreqHz{0};
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
