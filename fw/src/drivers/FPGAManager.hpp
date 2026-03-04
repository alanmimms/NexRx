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

  static constexpr uint8_t REG_SCRATCHPAD = 0x00;
  static constexpr uint16_t VALIDATION_MAGIC = 0x55AA;

  static void init();
  static bool loadBitstream(const uint8_t* data, size_t len);

private:
  static bool validatePath(const struct device* spiDev);
};

} // namespace nexrx
