#pragma once

#include <zephyr/kernel.h>
#include <stdint.h>

namespace nexrx {

class NexBus {
public:
  enum class Mode {
    PULSE_WIDTH_1M, /* Mode A: Runtime */
    UART_115K,      /* Mode B: Firmware Update */
  };

  static constexpr uint32_t BIT_PERIOD_TICKS = 100;
  static constexpr uint32_t DUTY_0_TICKS = 33;
  static constexpr uint32_t DUTY_1_TICKS = 66;

  static uint32_t dmaBuffer[64];

  static void init();
  static void setMode(Mode mode);
  static void transmit(uint32_t data, uint8_t bitCount);
  static bool updateFirmware(const uint8_t* data, size_t len);
};

} // namespace nexrx
