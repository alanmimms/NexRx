#pragma once

#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>

namespace nexrx {

class Si5351Driver {
public:
  static constexpr uint8_t i2cAddress = 0x60;
  static constexpr uint32_t refClockHz = 40000000; /* 40 MHz TCXO Reference */

  static void init();

  /**
   * @brief Configures OSD0 (CLK0) and OSD1 (CLK1) synthesis frequencies.
   * @param freqHz Target VFO center frequency in Hz.
   * @param offsetHz Intermediate frequency offset in Hz (typically 12000).
   * @param isFourPhase True for 4-phase QSD (> 15 MHz), false for 8-phase OSD (< 15 MHz).
   * @return true on success, false on configuration error.
   */
  static bool setVFOFrequency(uint32_t freqHz, uint32_t offsetHz, bool isFourPhase = false);

  /**
   * @brief Globally enables or disables Si5351 clock outputs.
   */
  static void outputEnable(bool enable);

private:
  static const struct device* getI2CDevice();
  static int writeRegister(uint8_t reg, uint8_t val);
  static int writeRegisters(uint8_t startReg, const uint8_t* data, size_t len);
  static bool configurePll(uint8_t pll, uint32_t pllFreqHz);
  static bool configureMultisynth(uint8_t msIndex, uint32_t pllFreqHz, uint32_t outFreqHz);
};

} // namespace nexrx
