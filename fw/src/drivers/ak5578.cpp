#include "AK5578.hpp"
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(nexrx_main, LOG_LEVEL_INF);

namespace nexrx {

void AK5578::init() {
  const struct device* i2cDev = DEVICE_DT_GET(DT_NODELABEL(i2c4));
  if (!device_is_ready(i2cDev)) return;

  LOG_INF("Codec: Performing Hard Reset...");
  gpio_pin_configure_dt(&resetPin, GPIO_OUTPUT_INACTIVE);
  gpio_pin_set_dt(&resetPin, 0);
  k_sleep(K_MSEC(10));
  gpio_pin_set_dt(&resetPin, 1);
  k_sleep(K_MSEC(10));

  LOG_INF("Codec: Initializing AK5578...");
  i2c_reg_write_byte(i2cDev, I2C_ADDR, 0x00, 0x00);
  i2c_reg_write_byte(i2cDev, I2C_ADDR, 0x02, 0x01);
  LOG_INF("Codec: Initialization Complete (4-lane I2S mode)");
}

} // namespace nexrx
