#include "fpga-manager.hpp"
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

  uint8_t dummy[13] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
                       0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
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

int FPGAManager::writeRegister(uint8_t addr, uint32_t data) {
  const struct device* spiDev = DEVICE_DT_GET(DT_NODELABEL(spi3));
  uint8_t txData[5];
  txData[0] = 0x80 | (addr & 0x7F);
  txData[1] = (data >> 24) & 0xFF;
  txData[2] = (data >> 16) & 0xFF;
  txData[3] = (data >> 8) & 0xFF;
  txData[4] = data & 0xFF;

  struct spi_config config = {
    .frequency = 10000000,
    .operation = SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(8),
    .slave = 0,
  };

  struct spi_buf txBuf = { .buf = txData, .len = sizeof(txData) };
  struct spi_buf_set txBufs = { .buffers = &txBuf, .count = 1 };

  gpio_pin_set_dt(&nss, 0);
  int ret = spi_write(spiDev, &config, &txBufs);
  gpio_pin_set_dt(&nss, 1);
  return ret;
}

int FPGAManager::readRegister(uint8_t addr, uint32_t &data) {
  const struct device* spiDev = DEVICE_DT_GET(DT_NODELABEL(spi3));
  uint8_t txData[5] = { (uint8_t)(addr & 0x7F), 0, 0, 0, 0 };
  uint8_t rxData[5] = {0};

  struct spi_config config = {
    .frequency = 10000000,
    .operation = SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(8),
    .slave = 0,
  };

  struct spi_buf txBuf = { .buf = txData, .len = sizeof(txData) };
  struct spi_buf_set txBufs = { .buffers = &txBuf, .count = 1 };
  struct spi_buf rxBuf = { .buf = rxData, .len = sizeof(rxData) };
  struct spi_buf_set rxBufs = { .buffers = &rxBuf, .count = 1 };

  gpio_pin_set_dt(&nss, 0);
  int ret = spi_transceive(spiDev, &config, &txBuf, &rxBufs);
  gpio_pin_set_dt(&nss, 1);

  if (ret == 0) {
    data = (rxData[1] << 24) | (rxData[2] << 16) | (rxData[3] << 8) | rxData[4];
  }
  return ret;
}

void FPGAManager::setISGIncrement(uint32_t inc) {
  writeRegister(regISGInc, inc);
}

void FPGAManager::setQSDIncrement(int lane, uint32_t inc) {
  uint8_t addr = regQSD0Inc;
  if (lane == 1) addr = regQSD1Inc;
  if (lane == 2) addr = regQSD2Inc;
  writeRegister(addr, inc);
}

void FPGAManager::commit(bool freq, bool phase) {
  uint32_t val = (freq ? 0x01 : 0x00) | (phase ? 0x02 : 0x00);
  writeRegister(regCommit, val);
}

uint64_t FPGAManager::getTCXOTimestamp() {
  uint32_t low, high;
  readRegister(regTimeL, low);
  readRegister(regTimeH, high);
  return (static_cast<uint64_t>(high) << 32) | low;
}

bool FPGAManager::validatePath(const struct device* spiDev) {
  uint32_t id, readVal;
  
  /* Verify FPGA ID */
  readRegister(regID, id);
  if (id != expectedID) {
    LOG_ERR("FPGA: ID mismatch! Expected 0x%08X, got 0x%08X", expectedID, id);
    return false;
  }

  /* Verify register read/write */
  writeRegister(regScratch, 0x55AAAA55);
  readRegister(regScratch, readVal);

  if (readVal == 0x55AAAA55) {
    LOG_INF("FPGA: SPI Path Validated (ID=0x%08X)", id);
    return true;
  }
  return false;
}

} // namespace nexrx
