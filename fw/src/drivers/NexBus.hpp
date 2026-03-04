#pragma once

#include <zephyr/kernel.h>
#include <stdint.h>

namespace nexrx {

class NexBus {
public:
  static constexpr uint32_t BIT_PERIOD_TICKS = 100;
  static constexpr uint32_t DUTY_0_TICKS = 33;
  static constexpr uint32_t DUTY_1_TICKS = 66;

  static uint32_t dmaBuffer[64];

  static void init();
  static void transmit(uint32_t data, uint8_t bitCount);
};

} // namespace nexrx
