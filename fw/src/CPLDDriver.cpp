#include "CPLDDriver.hpp"
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>

LOG_MODULE_REGISTER(cpld_driver, LOG_LEVEL_INF);

namespace nexrx {

namespace {

const struct gpio_dt_spec resetGpio = GPIO_DT_SPEC_GET_OR(DT_NODELABEL(cpld0), reset_gpios, {0});
const struct gpio_dt_spec doneGpio = GPIO_DT_SPEC_GET_OR(DT_NODELABEL(cpld0), done_gpios, {0});
const struct gpio_dt_spec nss0Gpio = GPIO_DT_SPEC_GET_OR(DT_NODELABEL(cpld0), nss_gpios, {0});
const struct gpio_dt_spec nss1Gpio = GPIO_DT_SPEC_GET_OR(DT_NODELABEL(cpld1), nss_gpios, {0});

} // namespace

const struct device* CPLDDriver::getSPIDevice() {
  static const struct device* dev = DEVICE_DT_GET(DT_NODELABEL(spi3));
  return dev;
}

int CPLDDriver::writeRegister(uint8_t addr, uint32_t data, int cpldIndex) {
  const struct device* dev = getSPIDevice();
  if (!device_is_ready(dev)) {
    return -ENODEV;
  }
  uint8_t txBuf[5] = {
    addr,
    static_cast<uint8_t>((data >> 24) & 0xFF),
    static_cast<uint8_t>((data >> 16) & 0xFF),
    static_cast<uint8_t>((data >> 8) & 0xFF),
    static_cast<uint8_t>(data & 0xFF)
  };
  struct spi_buf buf = {.buf = txBuf, .len = sizeof(txBuf)};
  struct spi_buf_set tx = {.buffers = &buf, .count = 1};

  struct spi_config config = {
    .frequency = 10000000,
    .slave = static_cast<uint16_t>((cpldIndex >= 0) ? cpldIndex : 0),
  };

  if (cpldIndex < 0) {
    // Broadcast to both CPLDs simultaneously
    if (device_is_ready(nss0Gpio.port)) gpio_pin_set_dt(&nss0Gpio, 1);
    if (device_is_ready(nss1Gpio.port)) gpio_pin_set_dt(&nss1Gpio, 1);
    int ret = spi_write(dev, &config, &tx);
    if (device_is_ready(nss0Gpio.port)) gpio_pin_set_dt(&nss0Gpio, 0);
    if (device_is_ready(nss1Gpio.port)) gpio_pin_set_dt(&nss1Gpio, 0);
    return ret;
  }

  return spi_write(dev, &config, &tx);
}

int CPLDDriver::readRegister(uint8_t addr, uint32_t &data, int cpldIndex) {
  const struct device* dev = getSPIDevice();
  if (!device_is_ready(dev)) {
    return -ENODEV;
  }
  uint8_t txBuf[5] = {static_cast<uint8_t>(addr | 0x80), 0, 0, 0, 0};
  uint8_t rxBuf[5] = {0};

  struct spi_buf txStruct = {.buf = txBuf, .len = sizeof(txBuf)};
  struct spi_buf rxStruct = {.buf = rxBuf, .len = sizeof(rxBuf)};
  struct spi_buf_set tx = {.buffers = &txStruct, .count = 1};
  struct spi_buf_set rx = {.buffers = &rxStruct, .count = 1};

  struct spi_config config = {
    .frequency = 10000000,
    .operation = SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(8),
    .slave = static_cast<uint16_t>(cpldIndex),
  };

  int ret = spi_transceive(dev, &config, &tx, &rx);
  if (ret == 0) {
    data = (static_cast<uint32_t>(rxBuf[1]) << 24) |
           (static_cast<uint32_t>(rxBuf[2]) << 16) |
           (static_cast<uint32_t>(rxBuf[3]) << 8)  |
           (static_cast<uint32_t>(rxBuf[4]));
  }
  return ret;
}

void CPLDDriver::init() {
  LOG_INF("Initializing Dual CPLD Interface...");
  if (device_is_ready(resetGpio.port)) {
    gpio_pin_configure_dt(&resetGpio, GPIO_OUTPUT_INACTIVE);
  }
  if (device_is_ready(doneGpio.port)) {
    gpio_pin_configure_dt(&doneGpio, GPIO_INPUT);
  }
  if (device_is_ready(nss0Gpio.port)) {
    gpio_pin_configure_dt(&nss0Gpio, GPIO_OUTPUT_INACTIVE);
  }
  if (device_is_ready(nss1Gpio.port)) {
    gpio_pin_configure_dt(&nss1Gpio, GPIO_OUTPUT_INACTIVE);
  }

  if (isConfigured()) {
    LOG_INF("CPLD already configured, validating signatures...");
    readBothSignatures();
  } else {
    LOG_INF("CPLDs unconfigured. Awaiting host bitstream upload.");
  }
}

bool CPLDDriver::isConfigured() {
  if (device_is_ready(doneGpio.port)) {
    return (gpio_pin_get_dt(&doneGpio) > 0);
  }
  return false;
}

bool CPLDDriver::programBitstream(const uint8_t* data, size_t length) {
  if (!data || length == 0) {
    LOG_ERR("CPLD Program: Invalid bitstream data");
    return false;
  }

  const struct device* spiDev = getSPIDevice();
  if (!device_is_ready(spiDev)) {
    LOG_ERR("CPLD Program: SPI3 device not ready");
    return false;
  }

  LOG_INF("CPLD Program: Starting slave SPI configuration (%u bytes)...", static_cast<unsigned int>(length));

  // 1. Enter Configuration Mode: Pulse CRESET low
  gpio_pin_set_dt(&resetGpio, 1); // Active low asserted
  gpio_pin_set_dt(&nss0Gpio, 1);  // Hold both NSS low
  gpio_pin_set_dt(&nss1Gpio, 1);
  k_busy_wait(500); // 500 us reset pulse

  // 2. Release CRESET while keeping NSS low
  gpio_pin_set_dt(&resetGpio, 0); // Active low deasserted
  k_busy_wait(1500); // Wait > 1200 us for internal housekeeping / RAM clear

  // 3. Stream bitstream over SPI3 simultaneously to both CPLDs
  struct spi_config config = {
    .frequency = 10000000, // 10 MHz
    .operation = SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(8),
    .slave = 0,
  };

  struct spi_buf txBuf = { .buf = const_cast<uint8_t*>(data), .len = length };
  struct spi_buf_set txSet = { .buffers = &txBuf, .count = 1 };
  int ret = spi_write(spiDev, &config, &txSet);
  if (ret != 0) {
    LOG_ERR("CPLD Program: SPI write failed (%d)", ret);
    return false;
  }

  // 4. Send dummy clocks (16 bytes = 128 clock cycles) while monitoring DONE
  uint8_t dummy[16] = {0};
  struct spi_buf dummyBuf = { .buf = dummy, .len = sizeof(dummy) };
  struct spi_buf_set dummySet = { .buffers = &dummyBuf, .count = 1 };
  spi_write(spiDev, &config, &dummySet);

  // 5. Deassert chip selects high to transition CPLDs to user mode
  gpio_pin_set_dt(&nss0Gpio, 0);
  gpio_pin_set_dt(&nss1Gpio, 0);
  k_sleep(K_MSEC(2));

  // 6. Check DONE pin
  bool done = isConfigured();
  if (!done) {
    LOG_ERR("CPLD Program: DONE pin did not assert!");
    return false;
  }

  LOG_INF("CPLD Program: DONE asserted successfully.");

  // 7. Validate user register signatures on both CPLDs
  return readBothSignatures();
}

bool CPLDDriver::readSignature(int cpldIndex) {
  uint32_t sig = 0;
  if (readRegister(regSig, sig, cpldIndex) == 0) {
    if (sig == expectedSig) {
      LOG_INF("CPLD %d Signature Validated: 0x%08X (NxRx)", cpldIndex, sig);
      return true;
    }
    LOG_WRN("CPLD %d Signature Mismatch: 0x%08X (Expected 0x%08X)", cpldIndex, sig, expectedSig);
  }
  return false;
}

bool CPLDDriver::readBothSignatures() {
  bool ok0 = readSignature(0);
  bool ok1 = readSignature(1);
  return (ok0 && ok1);
}

void CPLDDriver::setClockMode(bool isFourPhase) {
  uint32_t ctrl = isFourPhase ? 0x01 : 0x00;
  LOG_INF("Setting CPLD Clock Mode: %s", isFourPhase ? "4-Phase QSD" : "8-Phase OSD");
  writeRegister(regControl, ctrl, -1); // Broadcast to both CPLDs
}

uint64_t CPLDDriver::getLatchedTCXOCount(int cpldIndex) {
  uint32_t hi = 0, lo = 0;
  readRegister(regPPSLatchHi, hi, cpldIndex);
  readRegister(regPPSLatchLo, lo, cpldIndex);
  return (static_cast<uint64_t>(hi) << 32) | lo;
}

} // namespace nexrx
