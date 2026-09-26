#pragma once

#include <stdint.h>
#include <atomic>

namespace nexrx {

class CPLDModel {
public:
  CPLDModel() 
    : octMode(true)
    , hysteresisState(ModeState::Octature)
    , tcxoCounter(0)
    , ppsLatchedValue(0) {}

  enum class ModeState {
    Octature,  // < 15 MHz (8-phase)
    Quadrature // > 15 MHz (4-phase)
  };

  void updateWaterfallViewport(double minFreqHz, double maxFreqHz) {
    if (minFreqHz > 15.0e6) {
      // Viewport strictly above 15 MHz -> Quadrature (4-phase QSD)
      octMode.store(false, std::memory_order_relaxed);
      hysteresisState = ModeState::Quadrature;
    } else if (maxFreqHz < 15.0e6) {
      // Viewport strictly below 15 MHz -> Octature (8-phase OSD)
      octMode.store(true, std::memory_order_relaxed);
      hysteresisState = ModeState::Octature;
    }
    // If viewport straddles 15 MHz, preserve current hysteresisState!
  }

  bool isOctMode() const {
    return octMode.load(std::memory_order_relaxed);
  }

  double getModeGainScale() const {
    // +3 dB gain normalization scaling for 4-phase QSD mode (sqrt(2) approx 1.41421356)
    return isOctMode() ? 1.0 : 1.4142135623730951;
  }

  void onPPSEvent() {
    ppsLatchedValue.store(tcxoCounter.load(std::memory_order_relaxed), std::memory_order_relaxed);
  }

  void incrementTCXOCounter(uint64_t ticks) {
    tcxoCounter.fetch_add(ticks, std::memory_order_relaxed);
  }

  uint64_t getLatchedTCXOValue() const {
    return ppsLatchedValue.load(std::memory_order_relaxed);
  }

private:
  std::atomic<bool> octMode;
  ModeState hysteresisState;
  std::atomic<uint64_t> tcxoCounter;
  std::atomic<uint64_t> ppsLatchedValue;
};

} // namespace nexrx
