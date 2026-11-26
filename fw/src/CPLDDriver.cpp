#include "CPLDDriver.hpp"
#include "defaultCPLDBitstream.hpp"
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>

LOG_MODULE_REGISTER(cpld_driver, LOG_LEVEL_INF);

namespace nexrx {

namespace {

const struct gpio_dt_spec cresetGPIO = GPIO_DT_SPEC_GET_OR(DT_NODELABEL(cpld0), reset_gpios, {0});
const struct gpio_dt_spec doneGPIO = GPIO_DT_SPEC_GET_OR(DT_NODELABEL(cpld0), done_gpios, {0});
const struct gpio_dt_spec nss0GPIO = GPIO_DT_SPEC_GET_OR(DT_NODELABEL(cpld0), nss_gpios, {0});
const struct gpio_dt_spec nss1GPIO = GPIO_DT_SPEC_GET_OR(DT_NODELABEL(cpld1), nss_gpios, {0});

} // namespace

const struct device* CPLDDriver::getSPIDevice() {
  static const struct device* dev = DEVICE_DT_GET(DT_ALIAS(cpld_spi));
  return dev;
}

int CPLDDriver::writeRegister(uint8_t addr, uint32_t data, int cpldIndex) {
  const struct device* dev = getSPIDevice();
  // CPLD SPI Protocol: Bit 7 = 1 for Write, Bits [6:0] = Register Address
  uint8_t txBuf[5] = {
    static_cast<uint8_t>((addr & 0x7F) | 0x80),
    static_cast<uint8_t>((data >> 24) & 0xFF),
    static_cast<uint8_t>((data >> 16) & 0xFF),
    static_cast<uint8_t>((data >> 8) & 0xFF),
    static_cast<uint8_t>(data & 0xFF)
  };
  struct spi_buf buf = {.buf = txBuf, .len = sizeof(txBuf)};
  struct spi_buf_set tx = {.buffers = &buf, .count = 1};

  struct spi_config config = {
    .frequency = 1700000,	// XXX could be 10MHz later
    .operation = SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(8),
    .slave = 0,
  };

  if (cpldIndex != 0 && cpldIndex != 1 && cpldIndex != BOTH_CPLDS) return -EINVAL;
  if (cpldIndex == 0 || cpldIndex == BOTH_CPLDS) gpio_pin_set_dt(&nss0GPIO, 1);
  if (cpldIndex == 1 || cpldIndex == BOTH_CPLDS) gpio_pin_set_dt(&nss1GPIO, 1);
  int ret = spi_write(dev, &config, &tx);
  gpio_pin_set_dt(&nss0GPIO, 0);
  gpio_pin_set_dt(&nss1GPIO, 0);
  return ret;
}

int CPLDDriver::readRegister(uint8_t addr, uint32_t &data, int cpldIndex) {
  const struct device* dev = getSPIDevice();
  // CPLD SPI Protocol: Bit 7 = 0 for Read, Bits [6:0] = Register Address
  uint8_t txBuf[] = {static_cast<uint8_t>(addr & 0x7Fu), 0, 0, 0, 0};
  uint8_t rxBuf[] = {0, 0, 0, 0, 0};

  struct spi_buf txStruct = {.buf = txBuf, .len = sizeof(txBuf)};
  struct spi_buf rxStruct = {.buf = rxBuf, .len = sizeof(rxBuf)};
  struct spi_buf_set tx = {.buffers = &txStruct, .count = 1};
  struct spi_buf_set rx = {.buffers = &rxStruct, .count = 1};

  struct spi_config config = {
    .frequency = 1700000,	// XXX this could be 10MHz
    .operation = SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(8),
    .slave = 0,
  };

  auto nssGPIOPtr = cpldIndex == 0 ? &nss0GPIO : &nss1GPIO;
  gpio_pin_set_dt(nssGPIOPtr, 1);
  int ret = spi_transceive(dev, &config, &tx, &rx);
  gpio_pin_set_dt(nssGPIOPtr, 0);

  data = (static_cast<uint32_t>(rxBuf[1]) << 24) |
    (static_cast<uint32_t>(rxBuf[2]) << 16) |
    (static_cast<uint32_t>(rxBuf[3]) << 8)  |
    (static_cast<uint32_t>(rxBuf[4]));

  return ret;
}

void CPLDDriver::init() {
  LOG_INF("Initializing Dual CPLD Interface...");
  if (device_is_ready(cresetGPIO.port)) {
    gpio_pin_configure_dt(&cresetGPIO, GPIO_OUTPUT_INACTIVE);
  }
  if (device_is_ready(doneGPIO.port)) {
    gpio_pin_configure_dt(&doneGPIO, GPIO_INPUT);
  }
  if (device_is_ready(nss0GPIO.port)) {
    gpio_pin_configure_dt(&nss0GPIO, GPIO_OUTPUT_INACTIVE);
  }
  if (device_is_ready(nss1GPIO.port)) {
    gpio_pin_configure_dt(&nss1GPIO, GPIO_OUTPUT_INACTIVE);
  }

  LOG_INF("Programming default embedded bitstream...");
  programDefaultBitstream();
}

bool CPLDDriver::isConfigured() {
  if (device_is_ready(doneGPIO.port)) {
    return (gpio_pin_get_dt(&doneGPIO) > 0);
  }
  return false;
}

bool CPLDDriver::programDefaultBitstream() {
  LOG_INF("Programming embedded default bitstream (%u bytes)...",
          static_cast<unsigned int>(defaultCPLDBitstreamLen));
  return programBitstream(defaultCPLDBitstream, defaultCPLDBitstreamLen);
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

  if (!device_is_ready(cresetGPIO.port) ||
      !device_is_ready(nss0GPIO.port) ||
      !device_is_ready(nss1GPIO.port))
  {
    LOG_ERR("CPLD Program: CPLD GPIOs not ready");
    return false;
  }

  LOG_INF("CPLD Program: Starting slave SPI configuration (%u bytes)...",
	  static_cast<unsigned>(length));

  // Enter Configuration Mode: Pulse CRESET low
  gpio_pin_set_dt(&cresetGPIO, 1); // Active low asserted
  gpio_pin_set_dt(&nss0GPIO, 1);  // Hold both NSS low
  gpio_pin_set_dt(&nss1GPIO, 1);
  k_sleep(K_MSEC(1));		// >= 500 us reset pulse

  // Release CRESET while keeping NSS low
  gpio_pin_set_dt(&cresetGPIO, 0); // Active low deasserted
  k_sleep(K_MSEC(2)); // Wait > 1200 us for internal housekeeping / RAM clear

  // Stream bitstream over SPI3 simultaneously to both CPLDs
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

  // Send dummy clocks (16 bytes = 128 clock cycles) while monitoring DONE
  uint8_t dummy[16] = {0};
  struct spi_buf dummyBuf = { .buf = dummy, .len = sizeof(dummy) };
  struct spi_buf_set dummySet = { .buffers = &dummyBuf, .count = 1 };
  spi_write(spiDev, &config, &dummySet);

  // Check DONE pin
  bool done = isConfigured();
  if (!done) {
    LOG_ERR("CPLD Program: DONE pin did not assert!");
    return false;
  }

  // Deassert chip selects high
  gpio_pin_set_dt(&nss0GPIO, 0);
  gpio_pin_set_dt(&nss1GPIO, 0);
  k_sleep(K_MSEC(2));

  LOG_INF("CPLD Program: DONE asserted successfully.");

  // Send dummy clocks with NSS high to transition CPLDs to user mode
  spi_write(spiDev, &config, &dummySet);
  k_sleep(K_MSEC(2));

  // Validate user register signatures on both CPLDs
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
  writeRegister(regControl, ctrl, BOTH_CPLDS); // Broadcast to both CPLDs
}

uint32_t CPLDDriver::getTCXOCounter(int cpldIndex) {
  uint32_t count = 0;
  readRegister(regCounter, count, cpldIndex);
  return count;
}

} // namespace nexrx
