#include "agc-manager.hpp"
#include <zephyr/logging/log.h>
#include <cmath>
#include "../drivers/max9939.hpp"
#include "../drivers/nexbus.hpp"

LOG_MODULE_DECLARE(nexrx_main, LOG_LEVEL_INF);

namespace nexrx {

AGCManager::Mode AGCManager::currentMode = Mode::MANUAL;
float AGCManager::targetHeadroomDB = -15.0f;
int32_t AGCManager::currentPgaCode = 0;
int32_t AGCManager::currentAttenDB = 0;

void AGCManager::init() {
  currentMode = Mode::MANUAL;
  currentPgaCode = 0;
  currentAttenDB = 0;
  applyHardwareGain();
}

void AGCManager::setMode(Mode mode) {
  currentMode = mode;
  LOG_INF("AGC: Mode set to %d", static_cast<int>(mode));
}

void AGCManager::processReflex(int32_t peakValue) {
  if (currentMode == Mode::MANUAL) return;

  float peakDb = 20.0f * std::log10(std::max(1, std::abs(peakValue)) / 8388607.0f);

  /* Fast Reflex: Immediate gain drop on potential clipping */
  if (peakDb > -3.0f) {
    /* TODO: Implement fast-attack reflex logic */
  }
}

void AGCManager::setTotalGain(float totalGainDB) {
  /* Coarse: NexBus Pads (0-45 dB in 3dB steps) */
  int32_t newAtten = static_cast<int32_t>(std::floor(totalGainDB / 3.0f) * 3.0f);
  newAtten = std::clamp(newAtten, 0, 45);

  /* Fine: PGA Gain steps (simulated as 1dB offsets here) */
  int32_t newPga = static_cast<int32_t>(totalGainDB - newAtten);
  newPga = std::clamp(newPga, 0, 11);

  currentAttenDB = newAtten;
  currentPgaCode = newPga;
  
  applyHardwareGain();
}

void AGCManager::applyHardwareGain() {
  /**
   * ATTENUATOR BITMASK MAPPING (Based on doc/rx-architecture.md):
   * Pad 3dB:  Bit 0
   * Pad 6dB:  Bit 1
   * Pad 12dB: Bit 2
   * Pad 24dB: Bit 3
   */
  uint32_t bitmask = 0;
  int32_t remaining = currentAttenDB;

  if (remaining >= 24) { bitmask |= (1 << 3); remaining -= 24; }
  if (remaining >= 12) { bitmask |= (1 << 2); remaining -= 12; }
  if (remaining >= 6)  { bitmask |= (1 << 1); remaining -= 6;  }
  if (remaining >= 3)  { bitmask |= (1 << 0); remaining -= 3;  }

  /* COORDINATED SWITCHING:
   * Start NexBus frame, wait ~30us for shift, then update SPI PGA.
   */
  NexBus::transmit(bitmask, 32); 
  
  /* Near-instant SPI write to align with switch completion */
  MAX9939::setGain(static_cast<uint8_t>(currentPgaCode));
}

} // namespace nexrx
