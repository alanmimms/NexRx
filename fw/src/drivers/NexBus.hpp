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

  /* 1MB/s symbol rate = 1us period. 
   * Each symbol carries 2 bits (4 states).
   * Duty cycles: 25%, 50%, 75%, 100%
   */
  static constexpr uint32_t BIT_PERIOD_TICKS = 100;
  static constexpr uint32_t DUTY_0_TICKS = 25;
  static constexpr uint32_t DUTY_1_TICKS = 50;
  static constexpr uint32_t DUTY_2_TICKS = 75;
  static constexpr uint32_t DUTY_3_TICKS = 100;
  static constexpr uint32_t SYNC_TICKS   = 300;

  static uint32_t dmaBuffer[64];

  static void init();
  static void setMode(Mode mode);
  
  /* Set state for a specific target (0-3). 4 bits per target. */
  static void setTarget(uint8_t id, uint8_t data);
  
  /* Transmit full 20-bit frame (4 bits * 4 targets + 4 mode bits) */
  static void commit();

  /* Legacy - for compatibility or direct bitstream */
  static void transmit(uint32_t data, uint8_t bitCount);
  
  static bool updateFirmware(const uint8_t* data, size_t len);

private:
  static uint8_t targetState[4];
};

} // namespace nexrx
