// NexRx Digital Twin - Attenuator Model
//
// Models the switched Pi-attenuator network (3, 6, 12, 24 dB stages).
//
// Copyright 2026 NexRx Project - MIT License

#pragma once

#include <atomic>
#include <cmath>

namespace nexrx {

class AttenuatorModel {
public:
  AttenuatorModel() {
    atten3dB.store(false);
    atten6dB.store(false);
    atten12dB.store(false);
    atten24dB.store(false);
  }

  void setAtten3dB(bool en) { atten3dB.store(en); }
  void setAtten6dB(bool en) { atten6dB.store(en); }
  void setAtten12dB(bool en) { atten12dB.store(en); }
  void setAtten24dB(bool en) { atten24dB.store(en); }

  double getTotalAttenDB() const {
    double total = 0;
    if (atten3dB.load()) total += 3.0;
    if (atten6dB.load()) total += 6.0;
    if (atten12dB.load()) total += 12.0;
    if (atten24dB.load()) total += 24.0;
    return total;
  }

  double getVoltageGain() const {
    return std::pow(10.0, -getTotalAttenDB() / 20.0);
  }

private:
  std::atomic<bool> atten3dB;
  std::atomic<bool> atten6dB;
  std::atomic<bool> atten12dB;
  std::atomic<bool> atten24dB;
};

} // namespace nexrx
