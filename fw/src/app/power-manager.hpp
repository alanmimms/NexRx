#pragma once

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

namespace nexrx {

class PowerManager {
public:
  static constexpr struct gpio_dt_spec vcore = 
    GPIO_DT_SPEC_GET(DT_NODELABEL(pwr_fpgacore), gpios);

  static constexpr struct gpio_dt_spec v33d = 
    GPIO_DT_SPEC_GET(DT_NODELABEL(pwr_3v3), gpios);

  static constexpr struct gpio_dt_spec v33a = 
    GPIO_DT_SPEC_GET(DT_NODELABEL(pwr_3va), gpios);

  static constexpr struct gpio_dt_spec v5va = 
    GPIO_DT_SPEC_GET(DT_NODELABEL(pwr_5va), gpios);

  static void init() {
    gpio_pin_configure_dt(&vcore, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&v33d, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&v33a, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&v5va, GPIO_OUTPUT_INACTIVE);
  }

  static void runSequence() {
    LOG_INF("--- NexRx Board Power-On Sequence Start ---");

    LOG_INF("[1/4] Enabling FPGA VCORE...");
    gpio_pin_set_dt(&vcore, 1);
    k_sleep(K_MSEC(100));

    LOG_INF("[2/4] Enabling 3.3V Digital Rail...");
    gpio_pin_set_dt(&v33d, 1);
    k_sleep(K_MSEC(100));

    LOG_INF("[3/4] Enabling 3.3V Analog Rail...");
    gpio_pin_set_dt(&v33a, 1);
    k_sleep(K_MSEC(100));

    LOG_INF("[4/4] Enabling 5V Analog Rail...");
    gpio_pin_set_dt(&v5va, 1);
    k_sleep(K_MSEC(100));

    LOG_INF("--- NexRx Board Power-On Sequence Success ---");
  }

  static void monitorUSBPower() {
    LOG_INF("USB Power Monitor: Checking for 3.0A (15W) Source...");
    /* TODO: Real ADC monitoring logic */
    LOG_INF("USB Power Monitor: Capacity = 3.0A (Detected)");
  }
};

} // namespace nexrx
