#pragma once

#include <zephyr/kernel.h>
#include <stdint.h>

namespace nexrx {

class TLV320ADC5140 {
public:
  static constexpr int NUM_DEVICES = 2; // OSD0 (slave 0) and OSD1 (slave 1)
  static constexpr int CHANNELS_PER_DEV = 4;
  static constexpr uint8_t MAX_GAIN_DB = 42;

  // Registers on Page 0
  static constexpr uint8_t REG_PAGE_SELECT = 0x00;
  static constexpr uint8_t REG_SW_RESET = 0x01;
  static constexpr uint8_t REG_SLEEP_CFG = 0x02;
  static constexpr uint8_t REG_ASI_CFG0 = 0x07;
  static constexpr uint8_t REG_CH1_CFG1 = 0x3D;
  static constexpr uint8_t REG_CH2_CFG1 = 0x42;
  static constexpr uint8_t REG_CH3_CFG1 = 0x47;
  static constexpr uint8_t REG_CH4_CFG1 = 0x4C;
  static constexpr uint8_t REG_IN_CH_EN = 0x73;
  static constexpr uint8_t REG_ASI_OUT_CH_EN = 0x74;
  static constexpr uint8_t REG_PWR_CFG = 0x75;

  static void init();
  static void reset();

  /**
   * @brief Sets the PGA gain for a specific channel on a device.
   * @param devIndex Device index (0 for OSD0, 1 for OSD1).
   * @param channel Channel index (0 to 3).
   * @param gainDB Analog PGA gain in dB (0 to 42 dB).
   * @return true on success, false on error.
   */
  static bool setChannelGain(int devIndex, int channel, uint8_t gainDB);

  /**
   * @brief Sets the PGA gain for all channels across both OSD ADCs.
   * @param gainDB Analog PGA gain in dB (0 to 42 dB).
   * @return true on success, false on error.
   */
  static bool setPGAGain(uint8_t gainDB);

  /**
   * @brief Writes a register on the specified device.
   */
  static bool writeRegister(int devIndex, uint8_t reg, uint8_t value);

  /**
   * @brief Reads a register from the specified device.
   */
  static bool readRegister(int devIndex, uint8_t reg, uint8_t* value);
};

} // namespace nexrx
