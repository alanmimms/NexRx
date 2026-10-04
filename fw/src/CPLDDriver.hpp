#pragma once

#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/spi.h>

namespace nexrx {

class CPLDDriver {
public:
  /* Register Addresses */
  static constexpr uint8_t regControl     = 0x00;
  static constexpr uint8_t regPPSLatchHi  = 0x01;
  static constexpr uint8_t regPPSLatchLo  = 0x02;
  static constexpr uint8_t regSig         = 0x7F;

  static constexpr uint32_t expectedSig   = 0x4E785278; /* "NxRx" */

  static void init();
  static bool isConfigured();
  static bool programBitstream(const uint8_t* data, size_t length);
  static bool readSignature(int cpldIndex = 0);
  static bool readBothSignatures();
  static void setClockMode(bool isFourPhase);
  static uint64_t getLatchedTCXOCount(int cpldIndex = 0);

private:
  static const struct device* getSPIDevice();
  static int writeRegister(uint8_t addr, uint32_t data, int cpldIndex = -1);
  static int readRegister(uint8_t addr, uint32_t &data, int cpldIndex = 0);
};

} // namespace nexrx
