#include "FPGAManager.hpp"
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(nexrx_main, LOG_LEVEL_INF);

namespace nexrx {

void FPGAManager::init() {
  gpio_pin_configure_dt(&prog, GPIO_OUTPUT_INACTIVE);
  gpio_pin_configure_dt(&done, GPIO_INPUT);
  gpio_pin_configure_dt(&nss, GPIO_OUTPUT_INACTIVE);
}

bool FPGAManager::loadBitstream(const uint8_t* data, size_t len) {
  const struct device* spiDev = DEVICE_DT_GET(DT_NODELABEL(spi3));
  if (!device_is_ready(spiDev)) return false;

  LOG_INF("FPGA: Configuration Starting (%d bytes)...", len);
  gpio_pin_set_dt(&nss, 0);
  gpio_pin_set_dt(&prog, 0);
  k_sleep(K_MSEC(1));
  gpio_pin_set_dt(&prog, 1);
  k_sleep(K_MSEC(2));

  struct spi_config config = {
    .frequency = 10000000,
    .operation = SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(8),
    .slave = 0,
  };

  if (data && len > 0) {
    struct spi_buf txBuf = { .buf = (void*)data, .len = len };
    struct spi_buf_set txBufs = { .buffers = &txBuf, .count = 1 };
    spi_write(spiDev, &config, &txBufs);
  }

  uint8_t dummy[13] = {0xFF};
  struct spi_buf dBuf = { .buf = dummy, .len = sizeof(dummy) };
  struct spi_buf_set dBufs = { .buffers = &dBuf, .count = 1 };
  spi_write(spiDev, &config, &dBufs);
  gpio_pin_set_dt(&nss, 1);

  if (gpio_pin_get_dt(&done)) {
    LOG_INF("FPGA: Configuration SUCCESS (DONE=High)");
    k_sleep(K_MSEC(10));
    return validatePath(spiDev);
  } else {
    LOG_ERR("FPGA: Configuration FAILED (DONE=Low)");
    return false;
  }
}

bool FPGAManager::validatePath(const struct device* spiDev) {
  uint8_t txData[3] = { (uint8_t)(0x80 | REG_SCRATCHPAD), 
                         (uint8_t)(VALIDATION_MAGIC >> 8), 
                         (uint8_t)(VALIDATION_MAGIC & 0xFF) };
  uint8_t rxData[3] = {0};
  struct spi_config config = {
    .frequency = 10000000,
    .operation = SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(8),
    .slave = 0,
  };
  struct spi_buf txBuf = { .buf = txData, .len = sizeof(txData) };
  struct spi_buf_set txBufs = { .buffers = &txBuf, .count = 1 };
  gpio_pin_set_dt(&nss, 0);
  spi_write(spiDev, &config, &txBufs);
  gpio_pin_set_dt(&nss, 1);

  txData[0] = REG_SCRATCHPAD;
  struct spi_buf rxBuf = { .buf = rxData, .len = sizeof(rxData) };
  struct spi_buf_set rxBufs = { .buffers = &rxBuf, .count = 1 };
  gpio_pin_set_dt(&nss, 0);
  spi_transceive(spiDev, &config, &txBuf, &rxBufs);
  gpio_pin_set_dt(&nss, 1);

  uint16_t readVal = (rxData[1] << 8) | rxData[2];
  if (readVal == VALIDATION_MAGIC) {
    LOG_INF("FPGA: SPI Path Validated (0x%04X matches)", readVal);
    return true;
  }
  return false;
}

} // namespace nexrx
