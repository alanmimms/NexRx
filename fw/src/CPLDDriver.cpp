#include "CPLDDriver.hpp"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(cpld_driver, LOG_LEVEL_INF);

namespace nexrx {

const struct device* CPLDDriver::getSPIDevice() {
  static const struct device* dev = DEVICE_DT_GET(DT_NODELABEL(spi3));
  return dev;
}

int CPLDDriver::writeRegister(uint8_t addr, uint32_t data) {
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
  return spi_write(dev, nullptr, &tx);
}

int CPLDDriver::readRegister(uint8_t addr, uint32_t &data) {
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

  int ret = spi_transceive(dev, nullptr, &tx, &rx);
  if (ret == 0) {
    data = (static_cast<uint32_t>(rxBuf[1]) << 24) |
           (static_cast<uint32_t>(rxBuf[2]) << 16) |
           (static_cast<uint32_t>(rxBuf[3]) << 8)  |
           (static_cast<uint32_t>(rxBuf[4]));
  }
  return ret;
}

void CPLDDriver::init() {
  LOG_INF("Initializing Dual CPLD Driver...");
  if (readSignature()) {
    LOG_INF("CPLD SPI Communication Validated (NxRx).");
  } else {
    LOG_WRN("CPLD Signature Check Failed!");
  }
}

bool CPLDDriver::readSignature() {
  uint32_t sig = 0;
  if (readRegister(regSig, sig) == 0) {
    return (sig == expectedSig);
  }
  return false;
}

void CPLDDriver::setClockMode(bool isFourPhase) {
  uint32_t ctrl = isFourPhase ? 0x01 : 0x00;
  LOG_INF("Setting CPLD Clock Mode: %s", isFourPhase ? "4-Phase QSD" : "8-Phase OSD");
  writeRegister(regControl, ctrl);
}

uint64_t CPLDDriver::getLatchedTCXOCount() {
  uint32_t hi = 0, lo = 0;
  readRegister(regPPSLatchHi, hi);
  readRegister(regPPSLatchLo, lo);
  return (static_cast<uint64_t>(hi) << 32) | lo;
}

} // namespace nexrx
