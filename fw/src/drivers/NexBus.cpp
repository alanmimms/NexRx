#include "NexBus.hpp"
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/uart.h>

LOG_MODULE_DECLARE(nexrx_main, LOG_LEVEL_INF);

namespace nexrx {

uint32_t NexBus::dmaBuffer[64];
uint8_t NexBus::targetState[4] = {0, 0, 0, 0};

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

void NexBus::setTarget(uint8_t id, uint8_t data) {
  if (id < 4) {
    targetState[id] = data & 0x0F;
  }
}

void NexBus::commit() {
  /* 20 bits total: 
   * - 4 bits data * 4 targets = 16 bits
   * - 4 bits mode (including 1 bit txrx for all targets)
   * Encoded as 10 symbols (2 bits each) + 1 SYNC pulse.
   */
  int pos = 0;
  dmaBuffer[pos++] = SYNC_TICKS;

  /* Data symbols: 4 targets * 2 symbols each = 8 symbols */
  for (int i = 0; i < 4; ++i) {
    uint8_t val = targetState[i];
    /* Symbol 0: Bits [1:0], Symbol 1: Bits [3:2] */
    for (int s = 0; s < 2; ++s) {
      uint8_t bits = (val >> (s * 2)) & 0x03;
      switch (bits) {
        case 0: dmaBuffer[pos++] = DUTY_0_TICKS; break;
        case 1: dmaBuffer[pos++] = DUTY_1_TICKS; break;
        case 2: dmaBuffer[pos++] = DUTY_2_TICKS; break;
        case 3: dmaBuffer[pos++] = DUTY_3_TICKS; break;
      }
    }
  }

  /* Mode symbols: 4 bits = 2 symbols. 
   * Target bit 4 (txrx) is encoded into the mode bits.
   * Mode Bits: [0: ModeType, 1: txrx, 2: reserved, 3: reserved]
   */
  uint8_t modeBits = 0;
  if (targetState[0] & (1 << 4)) modeBits |= (1 << 1); 
  
  /* Symbol 9: Mode Bits [1:0], Symbol 10: Mode Bits [3:2] */
  for (int s = 0; s < 2; ++s) {
    uint8_t bits = (modeBits >> (s * 2)) & 0x03;
    switch (bits) {
      case 0: dmaBuffer[pos++] = DUTY_0_TICKS; break;
      case 1: dmaBuffer[pos++] = DUTY_1_TICKS; break;
      case 2: dmaBuffer[pos++] = DUTY_2_TICKS; break;
      case 3: dmaBuffer[pos++] = DUTY_3_TICKS; break;
    }
  }
  
  dmaBuffer[pos++] = 0; /* End of DMA sequence */

  /* TODO: Trigger TIM3 DMA update to actually send the pulse train */
  LOG_DBG("NexBus: Committed 20 bits");
}

void NexBus::transmit(uint32_t data, uint8_t bitCount) {
  /* Legacy bit-by-symbol transmission */
  int pos = 0;
  for (int i = 0; i < bitCount; ++i) {
    bool b = (data >> (bitCount - 1 - i)) & 0x01;
    dmaBuffer[pos++] = b ? DUTY_3_TICKS : DUTY_1_TICKS; /* 100% or 50% */
  }
  dmaBuffer[pos] = 0;
  LOG_DBG("NexBus: Transmitted 0x%08X (%d bits) legacy", data, bitCount);
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
