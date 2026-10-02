#include "AGCManager.hpp"
#include <zephyr/logging/log.h>
#include <cmath>
#include <algorithm>
#include "TLV320ADC5140.hpp"

LOG_MODULE_DECLARE(nexrx_main, LOG_LEVEL_INF);

namespace nexrx {

AGCManager::Mode AGCManager::currentMode = Mode::MANUAL;
float AGCManager::targetHeadroomDB = -15.0f;
int32_t AGCManager::currentPGACode = 0;
int32_t AGCManager::currentAttenDB = 0;

/* Internal Gain State (in dB) */
static float virtualGainDB = 0.0f;
static float lastPeakDB = -100.0f;

void AGCManager::init() {
  currentMode = Mode::MANUAL;
  virtualGainDB = 20.0f; /* Start with baseline gain */
  applyHardwareGain();
}

void AGCManager::setMode(Mode mode) {
  currentMode = mode;
  LOG_INF("AGC: Mode set to %d", static_cast<int>(mode));
}

void AGCManager::processReflex(int32_t peakValue) {
  if (currentMode == Mode::MANUAL) {
    return;
  }

  /* 1. Convert peak to dBFS */
  float peakDB = 20.0f * std::log10(std::max(1, std::abs(peakValue)) / 8388607.0f);
  lastPeakDB = peakDB;

  /* 2. Calculate Error relative to target headroom */
  float error = peakDB - targetHeadroomDB;

  /* 3. Apply Attack/Decay Timing */
  if (error > 0) {
    /* ATTACK: Drop gain fast */
    virtualGainDB -= error * 0.5f; 
  } else {
    /* DECAY: Increase gain slowly based on mode */
    float decayFactor = 0.001f;
    if (currentMode == Mode::FAST) {
      decayFactor = 0.05f;
    }
    if (currentMode == Mode::MEDIUM) {
      decayFactor = 0.01f;
    }
    virtualGainDB -= error * decayFactor;
  }

  /* 4. Clamp to hardware limits */
  virtualGainDB = std::clamp(virtualGainDB, 0.0f, 60.0f);

  /* 5. Update Hardware if change is significant (> 0.5 dB) */
  static float lastAppliedGain = -1.0f;
  if (std::abs(virtualGainDB - lastAppliedGain) > 0.5f) {
    setTotalGain(virtualGainDB);
    lastAppliedGain = virtualGainDB;
  }
}

void AGCManager::setTotalGain(float totalGainDB) {
  int32_t newAtten = static_cast<int32_t>(std::floor(totalGainDB / 3.0f) * 3.0f);
  newAtten = std::clamp(newAtten, 0, 45);

  int32_t newPGA = static_cast<int32_t>(totalGainDB - newAtten);
  newPGA = std::clamp(newPGA, 0, 42);

  currentAttenDB = newAtten;
  currentPGACode = newPGA;
  
  applyHardwareGain();
}

void AGCManager::applyHardwareGain() {
  TLV320ADC5140::setPGAGain(static_cast<uint8_t>(currentPGACode));
}

} // namespace nexrx
