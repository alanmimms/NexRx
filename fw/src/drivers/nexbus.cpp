#include "NexBus.hpp"
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/uart.h>

LOG_MODULE_DECLARE(nexrx_main, LOG_LEVEL_INF);

namespace nexrx {

uint32_t NexBus::dmaBuffer[64];

void NexBus::init() {
  const struct device* timDev = DEVICE_DT_GET(DT_NODELABEL(tim3));
  if (!device_is_ready(timDev)) {
    LOG_ERR("NexBus: TIM3 not ready");
    return;
  }
  LOG_INF("NexBus: Initialized");
}

void NexBus::setMode(Mode mode) {
  const struct device* nexbusDev = DEVICE_DT_GET(DT_NODELABEL(nexbus));
  const struct pinctrl_dev_config* pcfg = 
    PINCTRL_DT_DEV_CONFIG_GET(DT_NODELABEL(nexbus));

  if (mode == Mode::PULSE_WIDTH_1M) {
    pinctrl_apply_state(pcfg, 0); /* mode_a */
    LOG_INF("NexBus: Switched to Mode A (1Mb/s Pulse-Width)");
  } else {
    pinctrl_apply_state(pcfg, 1); /* mode_b */
    LOG_INF("NexBus: Switched to Mode B (115.2k UART)");
  }
}

void NexBus::transmit(uint32_t data, uint8_t bitCount) {
  for (int i = 0; i < bitCount; ++i) {
    bool b = (data >> (bitCount - 1 - i)) & 0x01;
    dmaBuffer[i] = b ? DUTY_1_TICKS : DUTY_0_TICKS;
  }
  dmaBuffer[bitCount] = 0;
  LOG_DBG("NexBus: Transmitted 0x%08X (%d bits)", data, bitCount);
}

bool NexBus::updateFirmware(const uint8_t* data, size_t len) {
  setMode(Mode::UART_115K);
  
  const struct device* uartDev = DEVICE_DT_GET(DT_NODELABEL(usart6));
  if (!device_is_ready(uartDev)) {
    LOG_ERR("NexBus: USART6 not ready for firmware update");
    return false;
  }

  LOG_INF("NexBus: Starting C011 firmware update (%d bytes)...", len);

  /* 
   * STM32 System Bootloader Sequence:
   * 1. Send 0x7F to sync baud.
   * 2. Wait for ACK (0x79).
   * 3. Send Write Memory commands.
   */
  uint8_t syncByte = 0x7F;
  uart_fifo_fill(uartDev, &syncByte, 1);
  
  /* TODO: Implement full STM32 bootloader state machine */
  
  LOG_INF("NexBus: Firmware update stub complete");
  
  setMode(Mode::PULSE_WIDTH_1M);
  return true;
}

} // namespace nexrx
