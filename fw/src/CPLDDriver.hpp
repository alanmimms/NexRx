#pragma once

#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/spi.h>

namespace nexrx {

class CPLDDriver {
public:
  /* Register Addresses */
  static constexpr uint8_t regControl = 0x00;
  static constexpr uint8_t regCounter = 0x01;
  static constexpr uint8_t regSig     = 0x7F;

  static constexpr uint32_t expectedSig = 0x4E785278; /* "NxRx" */
  static constexpr int BOTH_CPLDS = -1;

  static void init();
  static bool isConfigured();
  static bool programBitstream(const uint8_t* data, size_t length);
  static bool programDefaultBitstream();
  static bool readSignature(int cpldIndex = 0);
  static bool readBothSignatures();
  static void setClockMode(bool isFourPhase);
  static uint32_t getTCXOCounter(int cpldIndex = 0);
  static uint64_t getLatchedTCXOCount(int cpldIndex = 0) {
    return static_cast<uint64_t>(getTCXOCounter(cpldIndex));
  }

  static int writeRegister(uint8_t addr, uint32_t data, int cpldIndex = BOTH_CPLDS);
  static int readRegister(uint8_t addr, uint32_t &data, int cpldIndex);

private:
  static const struct device* getSPIDevice();
};

} // namespace nexrx
