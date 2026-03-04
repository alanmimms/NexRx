#include "NexBus.hpp"
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(nexrx_main, LOG_LEVEL_INF);

namespace nexrx {

uint32_t NexBus::dmaBuffer[64];

void NexBus::init() {
  const struct device* timDev = DEVICE_DT_GET(DT_NODELABEL(tim3));
  if (!device_is_ready(timDev)) {
    LOG_ERR("TIM3 not ready for NexBus");
    return;
  }
  LOG_INF("NexBus: Mode A Initialized (1Mb/s Pulse-Width)");
}

void NexBus::transmit(uint32_t data, uint8_t bitCount) {
  for (int i = 0; i < bitCount; ++i) {
    bool b = (data >> (bitCount - 1 - i)) & 0x01;
    dmaBuffer[i] = b ? DUTY_1_TICKS : DUTY_0_TICKS;
  }
  dmaBuffer[bitCount] = 0;
  LOG_DBG("NexBus: Transmitted 0x%08X (%d bits)", data, bitCount);
}

} // namespace nexrx
