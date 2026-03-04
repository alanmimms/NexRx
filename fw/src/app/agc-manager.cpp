#include "agc-manager.hpp"
#include <zephyr/logging/log.h>
#include <cmath>
#include <algorithm>
#include "../drivers/max9939.hpp"
#include "../drivers/nexbus.hpp"

LOG_MODULE_DECLARE(nexrx_main, LOG_LEVEL_INF);

namespace nexrx {

AGCManager::Mode AGCManager::currentMode = Mode::MANUAL;
float AGCManager::targetHeadroomDB = -15.0f;
int32_t AGCManager::currentPgaCode = 0;
int32_t AGCManager::currentAttenDB = 0;

/* Internal Gain State (in dB) */
static float virtualGainDB = 0.0f;
static float lastPeakDB = -100.0f;

void AGCManager::init() {
  currentMode = Mode::MANUAL;
  virtualGainDB = 20.0f; /* Start with some baseline gain */
  applyHardwareGain();
}

void AGCManager::setMode(Mode mode) {
  currentMode = mode;
  LOG_INF("AGC: Mode set to %d", static_cast<int>(mode));
}

void AGCManager::processReflex(int32_t peakValue) {
  if (currentMode == Mode::MANUAL) return;

  /* 1. Convert peak to dBFS */
  float peakDb = 20.0f * std::log10(std::max(1, std::abs(peakValue)) / 8388607.0f);
  lastPeakDB = peakDb;

  /* 2. Calculate Error relative to -15dBFS target */
  float error = peakDb - targetHeadroomDB;

  /* 3. Apply Attack/Decay Timing */
  if (error > 0) {
    /* ATTACK: Signal is too strong. Drop gain fast (1ms constant approx) */
    virtualGainDB -= error * 0.5f; 
  } else {
    /* DECAY: Signal is weak. Increase gain slowly based on mode */
    float decayFactor = 0.001f; /* Default Slow */
    if (currentMode == Mode::FAST)   decayFactor = 0.05f;
    if (currentMode == Mode::MEDIUM) decayFactor = 0.01f;
    
    virtualGainDB -= error * decayFactor;
  }

  /* 4. Clamp to hardware limits (approx 0 to 60 dB total range) */
  virtualGainDB = std::clamp(virtualGainDB, 0.0f, 60.0f);

  /* 5. Update Hardware if change is significant (> 0.5 dB) */
  static float lastAppliedGain = -1.0f;
  if (std::abs(virtualGainDB - lastAppliedGain) > 0.5f) {
    setTotalGain(virtualGainDB);
    lastAppliedGain = virtualGainDB;
  }
}

void AGCManager::setTotalGain(float totalGainDB) {
  /* 
   * Handover Logic: 
   * Map virtualGainDB to Attenuator (3dB steps) and PGA (fine).
   */
  int32_t newAtten = static_cast<int32_t>(std::floor(totalGainDB / 3.0f) * 3.0f);
  newAtten = std::clamp(newAtten, 0, 45);

  /* Map remaining gain to PGA code (approx 1dB per code for this model) */
  int32_t newPga = static_cast<int32_t>(totalGainDB - newAtten);
  newPga = std::clamp(newPga, 0, 11);

  currentAttenDB = newAtten;
  currentPgaCode = newPga;
  
  applyHardwareGain();
}

void AGCManager::applyHardwareGain() {
  uint32_t bitmask = 0;
  int32_t rem = currentAttenDB;
  if (rem >= 24) { bitmask |= (1 << 3); rem -= 24; }
  if (rem >= 12) { bitmask |= (1 << 2); rem -= 12; }
  if (rem >= 6)  { bitmask |= (1 << 1); rem -= 6;  }
  if (rem >= 3)  { bitmask |= (1 << 0); rem -= 3;  }

  /* COORDINATED REFLEX: Minimize gain glitches */
  NexBus::transmit(bitmask, 32); 
  MAX9939::setGain(static_cast<uint8_t>(currentPgaCode));
}

} // namespace nexrx
