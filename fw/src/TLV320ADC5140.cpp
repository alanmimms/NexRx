#include "TLV320ADC5140.hpp"
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <algorithm>

LOG_MODULE_REGISTER(TLV320ADC5140, LOG_LEVEL_INF);

namespace nexrx {

namespace {

const struct gpio_dt_spec adcResetGpio = GPIO_DT_SPEC_GET_OR(DT_NODELABEL(adc_reset), gpios, {0});

} // namespace

void TLV320ADC5140::init() {
  const struct device* spiDev = DEVICE_DT_GET(DT_NODELABEL(spi4));
  if (!device_is_ready(spiDev)) {
    LOG_ERR("TLV320ADC5140: SPI4 not ready");
    return;
  }

  reset();

  for (int dev = 0; dev < NUM_DEVICES; dev++) {
    // Select Page 0
    writeRegister(dev, REG_PAGE_SELECT, 0x00);

    // Software reset
    writeRegister(dev, REG_SW_RESET, 0x01);
    k_sleep(K_MSEC(10));

    // Wake device from sleep
    writeRegister(dev, REG_SLEEP_CFG, 0x00);

    // ASI configuration: 24-bit word length
    writeRegister(dev, REG_ASI_CFG0, 0x30);

    // Default PGA gain to 0 dB
    for (int ch = 0; ch < CHANNELS_PER_DEV; ch++) {
      setChannelGain(dev, ch, 0);
    }

    // Enable input channels 1-4
    writeRegister(dev, REG_IN_CH_EN, 0xF0);

    // Enable ASI output channels 1-4
    writeRegister(dev, REG_ASI_OUT_CH_EN, 0xF0);

    // Power up ADC cores, PLL, and MICBIAS
    writeRegister(dev, REG_PWR_CFG, 0xE0);

    LOG_INF("TLV320ADC5140: Device %d initialized on SPI4", dev);
  }
}

void TLV320ADC5140::reset() {
  if (device_is_ready(adcResetGpio.port)) {
    gpio_pin_configure_dt(&adcResetGpio, GPIO_OUTPUT_ACTIVE);
    gpio_pin_set_dt(&adcResetGpio, 1);
    k_sleep(K_MSEC(5));
    gpio_pin_set_dt(&adcResetGpio, 0);
    k_sleep(K_MSEC(10));
    LOG_INF("TLV320ADC5140: Hardware reset pulse complete");
  }
}

bool TLV320ADC5140::setChannelGain(int devIndex, int channel, uint8_t gainDB) {
  if (devIndex < 0 || devIndex >= NUM_DEVICES) {
    return false;
  }
  if (channel < 0 || channel >= CHANNELS_PER_DEV) {
    return false;
  }

  if (gainDB > MAX_GAIN_DB) {
    gainDB = MAX_GAIN_DB;
  }

  constexpr uint8_t chRegs[CHANNELS_PER_DEV] = {
    REG_CH1_CFG1,
    REG_CH2_CFG1,
    REG_CH3_CFG1,
    REG_CH4_CFG1
  };

  // Bits 7:2 configure analog PGA gain (0 to 42 dB)
  uint8_t regVal = static_cast<uint8_t>((gainDB & 0x3F) << 2);
  return writeRegister(devIndex, chRegs[channel], regVal);
}

bool TLV320ADC5140::setPGAGain(uint8_t gainDB) {
  bool allOk = true;
  for (int dev = 0; dev < NUM_DEVICES; dev++) {
    for (int ch = 0; ch < CHANNELS_PER_DEV; ch++) {
      if (!setChannelGain(dev, ch, gainDB)) {
        allOk = false;
      }
    }
  }
  if (allOk) {
    LOG_INF("TLV320ADC5140: Set all PGA gains to %u dB", gainDB);
  }
  return allOk;
}

bool TLV320ADC5140::writeRegister(int devIndex, uint8_t reg, uint8_t value) {
  const struct device* spiDev = DEVICE_DT_GET(DT_NODELABEL(spi4));
  if (!device_is_ready(spiDev)) {
    return false;
  }

  struct spi_config config = {
    .frequency = 10000000,
    .operation = SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(8),
    .slave = static_cast<uint16_t>(devIndex),
  };

  // Byte 0: Register address shifted left by 1 with R/W bit 0 (Write)
  // Byte 1: Value to write
  uint8_t tx[2] = {
    static_cast<uint8_t>((reg << 1) & 0xFE),
    value
  };

  struct spi_buf txBuf = { .buf = tx, .len = 2 };
  struct spi_buf_set txBufs = { .buffers = &txBuf, .count = 1 };

  return (spi_write(spiDev, &config, &txBufs) == 0);
}

bool TLV320ADC5140::readRegister(int devIndex, uint8_t reg, uint8_t* value) {
  if (value == nullptr) {
    return false;
  }

  const struct device* spiDev = DEVICE_DT_GET(DT_NODELABEL(spi4));
  if (!device_is_ready(spiDev)) {
    return false;
  }

  struct spi_config config = {
    .frequency = 10000000,
    .operation = SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(8),
    .slave = static_cast<uint16_t>(devIndex),
  };

  // Byte 0: Register address shifted left by 1 with R/W bit 1 (Read)
  // Byte 1: Dummy clock byte
  uint8_t tx[2] = {
    static_cast<uint8_t>((reg << 1) | 0x01),
    0x00
  };
  uint8_t rx[2] = { 0, 0 };

  struct spi_buf txBuf = { .buf = tx, .len = 2 };
  struct spi_buf_set txBufs = { .buffers = &txBuf, .count = 1 };
  struct spi_buf rxBuf = { .buf = rx, .len = 2 };
  struct spi_buf_set rxBufs = { .buffers = &rxBuf, .count = 1 };

  if (spi_transceive(spiDev, &config, &txBufs, &rxBufs) == 0) {
    *value = rx[1];
    return true;
  }

  return false;
}

} // namespace nexrx
