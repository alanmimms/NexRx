#include "Si5351Driver.hpp"
#include <zephyr/logging/log.h>
#include <algorithm>
#include <cmath>

LOG_MODULE_REGISTER(si5351, LOG_LEVEL_INF);

namespace nexrx {

namespace {

// Si5351 Register Map Constants
constexpr uint8_t REG_OUTPUT_ENABLE    = 3;
constexpr uint8_t REG_CLK0_CONTROL     = 16;
constexpr uint8_t REG_CLK1_CONTROL     = 17;
constexpr uint8_t REG_PLLA_PARAMETERS  = 26;
constexpr uint8_t REG_PLLB_PARAMETERS  = 34;
constexpr uint8_t REG_MS0_PARAMETERS   = 42;
constexpr uint8_t REG_MS1_PARAMETERS   = 50;
constexpr uint8_t REG_PLL_RESET        = 177;

// Helpers to compute GCD
uint32_t gcd(uint32_t a, uint32_t b) {
  while (b != 0) {
    uint32_t t = b;
    b = a % b;
    a = t;
  }
  return a;
}

} // namespace

const struct device* Si5351Driver::getI2CDevice() {
  static const struct device* dev = DEVICE_DT_GET(DT_NODELABEL(i2c1));
  return dev;
}

int Si5351Driver::writeRegister(uint8_t reg, uint8_t val) {
  const struct device* dev = getI2CDevice();
  if (!device_is_ready(dev)) {
    return -ENODEV;
  }
  uint8_t buf[2] = {reg, val};
  return i2c_write(dev, buf, 2, i2cAddress);
}

int Si5351Driver::writeRegisters(uint8_t startReg, const uint8_t* data, size_t len) {
  const struct device* dev = getI2CDevice();
  if (!device_is_ready(dev)) {
    return -ENODEV;
  }
  uint8_t buf[16];
  if (len + 1 > sizeof(buf)) {
    return -EINVAL;
  }
  buf[0] = startReg;
  for (int i = 0; i < static_cast<int>(len); ++i) {
    buf[i + 1] = data[i];
  }
  return i2c_write(dev, buf, len + 1, i2cAddress);
}

void Si5351Driver::init() {
  LOG_INF("Initializing Si5351 Synthesizer (40 MHz TCXO Reference)...");

  // Disable all outputs while configuring
  writeRegister(REG_OUTPUT_ENABLE, 0xFF);

  // Power down unused clocks, set CLK0 to use PLLA, CLK1 to use PLLB
  // 8mA drive strength, MultiSynth input, not inverted
  writeRegister(REG_CLK0_CONTROL, 0x4F); // PLLA, MultiSynth 0, 8mA
  writeRegister(REG_CLK1_CONTROL, 0x6F); // PLLB, MultiSynth 1, 8mA

  // Set default initial frequency (e.g. 14.2 MHz - 12 kHz on CLK0, + 12 kHz on CLK1)
  setVFOFrequency(14200000, 12000, false);

  outputEnable(true);
  LOG_INF("Si5351 Synthesizer Initialized.");
}

bool Si5351Driver::configurePll(uint8_t pllIndex, uint32_t pllFreqHz) {
  // Fpll = Fxtal * (a + b/c)
  uint32_t a = pllFreqHz / refClockHz;
  uint32_t remainder = pllFreqHz % refClockHz;
  uint32_t c = 1048575; // Max 20-bit fractional denominator
  uint32_t b = static_cast<uint32_t>((static_cast<uint64_t>(remainder) * c) / refClockHz);

  uint32_t divGcd = gcd(b, c);
  if (divGcd > 1) {
    b /= divGcd;
    c /= divGcd;
  }

  uint32_t p1 = 128 * a + static_cast<uint32_t>(std::floor(128.0 * b / c)) - 512;
  uint32_t p2 = 128 * b - c * static_cast<uint32_t>(std::floor(128.0 * b / c));
  uint32_t p3 = c;

  uint8_t regData[8];
  regData[0] = static_cast<uint8_t>((p3 >> 8) & 0xFF);
  regData[1] = static_cast<uint8_t>(p3 & 0xFF);
  regData[2] = static_cast<uint8_t>((p1 >> 16) & 0x03);
  regData[3] = static_cast<uint8_t>((p1 >> 8) & 0xFF);
  regData[4] = static_cast<uint8_t>(p1 & 0xFF);
  regData[5] = static_cast<uint8_t>(((p3 >> 16) & 0x0F) << 4 | ((p2 >> 16) & 0x0F));
  regData[6] = static_cast<uint8_t>((p2 >> 8) & 0xFF);
  regData[7] = static_cast<uint8_t>(p2 & 0xFF);

  uint8_t startReg = (pllIndex == 0) ? REG_PLLA_PARAMETERS : REG_PLLB_PARAMETERS;
  return (writeRegisters(startReg, regData, 8) == 0);
}

bool Si5351Driver::configureMultisynth(uint8_t msIndex, uint32_t pllFreqHz, uint32_t outFreqHz) {
  // Integer division: Fout = Fpll / div
  uint32_t div = pllFreqHz / outFreqHz;
  if (div < 6 || div > 1800) {
    LOG_ERR("Si5351: Invalid MultiSynth divider %u", div);
    return false;
  }

  uint32_t p1 = 128 * div - 512;
  uint32_t p2 = 0;
  uint32_t p3 = 1;

  uint8_t regData[8];
  regData[0] = static_cast<uint8_t>((p3 >> 8) & 0xFF);
  regData[1] = static_cast<uint8_t>(p3 & 0xFF);
  regData[2] = static_cast<uint8_t>((p1 >> 16) & 0x03) | 0x40; // Bit 6: integer mode
  regData[3] = static_cast<uint8_t>((p1 >> 8) & 0xFF);
  regData[4] = static_cast<uint8_t>(p1 & 0xFF);
  regData[5] = static_cast<uint8_t>(((p3 >> 16) & 0x0F) << 4 | ((p2 >> 16) & 0x0F));
  regData[6] = static_cast<uint8_t>((p2 >> 8) & 0xFF);
  regData[7] = static_cast<uint8_t>(p2 & 0xFF);

  uint8_t startReg = (msIndex == 0) ? REG_MS0_PARAMETERS : REG_MS1_PARAMETERS;
  return (writeRegisters(startReg, regData, 8) == 0);
}

bool Si5351Driver::setVFOFrequency(uint32_t freqHz, uint32_t offsetHz, bool isFourPhase) {
  // In 8-phase mode (< 15 MHz), CPLD requires 8x LO.
  // In 4-phase mode (> 15 MHz), CPLD requires 4x LO.
  uint32_t mult = isFourPhase ? 4 : 8;

  uint32_t osd0Base = (freqHz >= offsetHz) ? (freqHz - offsetHz) : 1000;
  uint32_t osd1Base = freqHz + offsetHz;

  uint32_t clk0Target = osd0Base * mult;
  uint32_t clk1Target = osd1Base * mult;

  // Choose target PLL frequency in 600 - 900 MHz
  // Select even divider div
  uint32_t div0 = std::clamp((700000000U / clk0Target) & ~1U, 6U, 254U);
  uint32_t pllA = clk0Target * div0;

  uint32_t div1 = std::clamp((700000000U / clk1Target) & ~1U, 6U, 254U);
  uint32_t pllB = clk1Target * div1;

  LOG_INF("Si5351 Tune: Center=%u Hz, Mode=%s (%ux) | CLK0=%u Hz (PLLA=%u, div=%u), CLK1=%u Hz (PLLB=%u, div=%u)",
          freqHz, isFourPhase ? "4-Phase" : "8-Phase", mult,
          clk0Target, pllA, div0, clk1Target, pllB, div1);

  bool okA = configurePll(0, pllA) && configureMultisynth(0, pllA, clk0Target);
  bool okB = configurePll(1, pllB) && configureMultisynth(1, pllB, clk1Target);

  // Reset PLLs to synchronize phases
  writeRegister(REG_PLL_RESET, 0xAC);

  return (okA && okB);
}

void Si5351Driver::outputEnable(bool enable) {
  // Register 3: 0 = enabled, 1 = disabled
  // Bit 0 = CLK0, Bit 1 = CLK1
  writeRegister(REG_OUTPUT_ENABLE, enable ? 0xFC : 0xFF);
  LOG_INF("Si5351 Outputs %s", enable ? "ENABLED" : "DISABLED");
}

} // namespace nexrx
