#pragma once

#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

namespace nexrx {

class FrontEndDriver {
public:
  static void init();

  /**
   * @brief Configures step attenuator array (0 to 45 dB in 3 dB steps).
   * @param attenDB Total attenuation requested.
   * @return Actual attenuation applied (clamped and rounded to 3 dB step).
   */
  static int setAttenuation(int attenDB);

  /**
   * @brief Selects the BPF branch and HPF state for the given RF frequency.
   * @param freqHz Center frequency in Hz.
   */
  static void selectFilterBand(uint32_t freqHz);

  /**
   * @brief Directly sets BPF branch (1 to 4) or 0 for bypass.
   * @param bpfIndex BPF branch (1: 1.8-3.4MHz, 2: 3.2-7.5MHz, 3: 7.3-14.5MHz, 4: 14.3-30MHz).
   */
  static void setBpfIndex(int bpfIndex);

  /**
   * @brief Enables or bypasses the broadcast AM high-pass filter.
   * @param bypass True to bypass HPF, false to insert HPF.
   */
  static void setHpfBypass(bool bypass);

  /**
   * @brief Enables or disables the 3.3V main power rail.
   * @param enable True to enable.
   */
  static void set3V3Power(bool enable);

  static int getAttenuation();
  static int getBpfIndex();
  static bool isHpfBypassed();

private:
  static int currentAttenDB;
  static int currentBpfIndex;
  static bool currentHpfBypass;
};

} // namespace nexrx
