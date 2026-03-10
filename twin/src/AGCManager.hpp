#pragma once

#include <stdint.h>
#include <cmath>
#include <algorithm>
#include "AttenuatorModel.hpp"
#include "sampler/RXControls.hpp"

namespace nexrx {

class AGCManager {
public:
  enum class Mode {
    FAST = 1,
    MEDIUM = 2,
    SLOW = 3,
    MANUAL = 0
  };

  AGCManager(AttenuatorModel* atten, PGAModel* pga) 
    : attenuator(atten), pgaModel(pga) {}

  void setMode(Mode mode) {
    currentMode = mode;
  }

  void setModeInt(int mode) {
    currentMode = static_cast<Mode>(mode);
  }

  void processReflex(int32_t peakValue) {
    if (currentMode == Mode::MANUAL) {
      return;
    }

    // 1. Convert peak to dBFS (24-bit)
    float peakDB = 20.0f * std::log10(std::max(1, std::abs(peakValue)) / 8388607.0f);

    // 2. Calculate Error relative to target headroom
    float error = peakDB - targetHeadroomDB;

    // 3. Apply Attack/Decay Timing
    if (error > 0) {
      // ATTACK: Signal is too strong. Drop gain fast.
      virtualGainDB -= error * 0.5f; 
    } else {
      // DECAY: Signal is weak. Increase gain slowly based on mode.
      float decayFactor = 0.001f; // Default Slow
      if (currentMode == Mode::FAST) {
        decayFactor = 0.05f;
      } else if (currentMode == Mode::MEDIUM) {
        decayFactor = 0.01f;
      }
      
      virtualGainDB -= error * decayFactor;
    }

    // 4. Clamp to hardware limits (approx 0 to 60 dB total range)
    virtualGainDB = std::clamp(virtualGainDB, 0.0f, 60.0f);

    // 5. Update Hardware if change is significant (> 0.2 dB for simulation smoothness)
    if (std::abs(virtualGainDB - lastAppliedGain) > 0.2f) {
      applyHardwareGain(virtualGainDB);
      lastAppliedGain = virtualGainDB;
    }
  }

private:
  AttenuatorModel* attenuator;
  PGAModel* pgaModel;
  Mode currentMode = Mode::MANUAL;
  float targetHeadroomDB = -15.0f;
  float virtualGainDB = 30.0f;
  float lastAppliedGain = -1.0f;

  void applyHardwareGain(float totalGainDB) {
    // Map virtualGainDB to Attenuator (3dB steps) and PGA.
    // Hardware attenuator is 0 to -45dB. Here we treat gain as positive.
    // So 60dB gain means 0dB attenuation and 60dB PGA (if possible).
    // Our models: Attenuator 0-45dB reduction. PGA 0-44dB gain approx.
    
    // We want to keep Attenuator at 0 (min attenuation) as long as possible for SNR.
    // If totalGainDB is high, attenuation is 0.
    // If totalGainDB is low, we need to add attenuation.
    
    float requiredAttenuation = std::max(0.0f, 45.0f - totalGainDB);
    int32_t attenCode = static_cast<int32_t>(std::floor(requiredAttenuation / 3.0f) * 3.0f);
    attenCode = std::clamp(attenCode, 0, 45);

    // Remaining gain goes to PGA
    float remainingGain = totalGainDB - (45.0f - attenCode);
    int32_t pgaCode = static_cast<int32_t>(std::round(remainingGain / 4.0f)); // Simple mapping
    pgaCode = std::clamp(pgaCode, 0, 11);

    attenuator->setTotalAttenuation(attenCode);
    pgaModel->setGainCode(pgaCode);
  }
};

} // namespace nexrx
