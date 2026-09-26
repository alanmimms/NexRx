#pragma once

#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>

namespace nexrx {

class Si5351Driver {
public:
  static constexpr uint8_t i2cAddress = 0x60;

  static void init();
  static bool setVFOFrequency(uint32_t freqHz, uint32_t offsetHz);
  static void outputEnable(bool enable);

private:
  static const struct device* getI2CDevice();
  static int writeRegister(uint8_t reg, uint8_t val);
};

} // namespace nexrx
