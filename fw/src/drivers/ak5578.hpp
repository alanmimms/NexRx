#pragma once

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <stdint.h>

namespace nexrx {

class AK5578 {
public:
  static constexpr uint8_t I2C_ADDR = 0x13;
  static constexpr struct gpio_dt_spec resetPin = 
    GPIO_DT_SPEC_GET(DT_NODELABEL(codec), reset_gpios);

  static void init();
};

} // namespace nexrx
