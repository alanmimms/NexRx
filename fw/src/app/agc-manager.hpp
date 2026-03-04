#pragma once

#include <zephyr/kernel.h>
#include <stdint.h>

namespace nexrx {

class AGCManager {
public:
  enum class Mode {
    FAST,
    MEDIUM,
    SLOW,
    MANUAL
  };

  static void init();
  static void setMode(Mode mode);
  
  /**
   * @brief High-priority reflex update.
   * Called by the QSDCapture thread after peak detection.
   * @param peakValue Maximum absolute value in the current buffer.
   */
  static void processReflex(int32_t peakValue);

  /**
   * @brief Sets total system gain in dB.
   * Internal logic coordinates NexBus pads and MAX9939 code.
   * @param totalGainDB Total gain relative to MDS.
   */
  static void setTotalGain(float totalGainDB);

private:
  static Mode currentMode;
  static float targetHeadroomDB; /* Default -15.0 dBFS */
  static int32_t currentPgaCode;
  static int32_t currentAttenDB;

  static void applyHardwareGain();
};

} // namespace nexrx
