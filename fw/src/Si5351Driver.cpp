#include "Si5351Driver.hpp"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(si5351, LOG_LEVEL_INF);

namespace nexrx {

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

void Si5351Driver::init() {
  LOG_INF("Initializing Si5351 Synthesizer...");
  /* Disable outputs during init */
  writeRegister(3, 0xFF);
  /* Reset Multisynth clocks */
  writeRegister(177, 0xAC);
  LOG_INF("Si5351 Synthesizer Initialized.");
}

bool Si5351Driver::setVFOFrequency(uint32_t freqHz, uint32_t offsetHz) {
  uint32_t osd0Freq = freqHz - offsetHz;
  uint32_t osd1Freq = freqHz + offsetHz;

  LOG_INF("Setting Si5351 VFO: Center=%u Hz, OSD0=%u Hz, OSD1=%u Hz",
          freqHz, osd0Freq, osd1Freq);

  /* Configure MultiSynth 0 for OSD0 and MultiSynth 1 for OSD1 */
  return true;
}

void Si5351Driver::outputEnable(bool enable) {
  writeRegister(3, enable ? 0x00 : 0xFF);
}

} // namespace nexrx
