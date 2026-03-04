#include "MAX9939.hpp"
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(nexrx_main, LOG_LEVEL_INF);

namespace nexrx {

void MAX9939::init() {
  const struct device* spiDev = DEVICE_DT_GET(DT_NODELABEL(spi4));
  if (!device_is_ready(spiDev)) {
    LOG_ERR("PGA: SPI4 not ready");
    return;
  }
  LOG_INF("PGA: Broadcast Gain Interface Initialized");
  setGain(0);
}

void MAX9939::setGain(uint8_t code) {
  const struct device* spiDev = DEVICE_DT_GET(DT_NODELABEL(spi4));
  uint8_t cmd = 0x80 | (code & 0x0F);
  struct spi_config config = {
    .frequency = 1000000,
    .operation = SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(8),
    .slave = 0,
  };
  struct spi_buf txBuf = { .buf = &cmd, .len = 1 };
  struct spi_buf_set txBufs = { .buffers = &txBuf, .count = 1 };
  if (spi_write(spiDev, &config, &txBufs) == 0) {
    LOG_INF("PGA: Broadcast Gain set to code 0x%01X", code);
  } else {
    LOG_ERR("PGA: SPI broadcast failed");
  }
}

} // namespace nexrx
