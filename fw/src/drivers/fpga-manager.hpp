#pragma once

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <stdint.h>

namespace nexrx {

class FPGAManager {
public:
  static constexpr struct gpio_dt_spec prog = 
    GPIO_DT_SPEC_GET(DT_NODELABEL(fpga), prog_gpios);

  static constexpr struct gpio_dt_spec done = 
    GPIO_DT_SPEC_GET(DT_NODELABEL(fpga), done_gpios);

  static constexpr struct gpio_dt_spec nss = 
    GPIO_DT_SPEC_GET(DT_NODELABEL(fpga), nss_gpios);

  /* Register Addresses (camelCase, no underscores) */
  static constexpr uint8_t regScratch = 0x00;
  static constexpr uint8_t regCommit  = 0x01;
  static constexpr uint8_t regID      = 0x02;
  static constexpr uint8_t regVersion = 0x03;
  static constexpr uint8_t regTimeL   = 0x04;
  static constexpr uint8_t regTimeH   = 0x05;
  static constexpr uint8_t regISGInc  = 0x10;
  static constexpr uint8_t regQSD0Inc = 0x20;
  static constexpr uint8_t regQSD1Inc = 0x30;
  static constexpr uint8_t regQSD2Inc = 0x40;

  static constexpr uint32_t expectedID = 0x4E585258; /* "NXRX" */
  static constexpr uint16_t validationMagic = 0x55AA;

  static void init();
  static bool loadBitstream(const uint8_t* data, size_t len);

  /* High-level Register Access */
  static int writeRegister(uint8_t addr, uint32_t data);
  static int readRegister(uint8_t addr, uint32_t &data);

  /* Frequency & Phase Control */
  static void setISGIncrement(uint32_t inc);
  static void setQSDIncrement(int lane, uint32_t inc);
  static void commit(bool freq, bool phase);

  /* Precision Timing */
  static uint64_t getTCXOTimestamp();

private:
  static bool validatePath(const struct device* spiDev);
};

} // namespace nexrx
