#pragma once

#include <stddef.h>
#include <stdint.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>

namespace nexrx {

class Si5351Driver {
public:
  enum class Pll : uint8_t {
    pllA = 0u,
    pllB = 1u
  };

  enum class ClockChannel : uint8_t {
    clk0 = 0u,
    clk1 = 1u,
    clk2 = 2u,
    clk3 = 3u,
    clk4 = 4u,
    clk5 = 5u,
    clk6 = 6u,
    clk7 = 7u
  };

  enum class DriveStrength : uint8_t {
    drive2mA = 0u,
    drive4mA = 1u,
    drive6mA = 2u,
    drive8mA = 3u
  };

  static constexpr uint8_t i2cAddress = 0x60u;
  static constexpr uint32_t nominalRefClockHz = 40000000u; // 40 MHz TCXO reference
  static constexpr uint32_t defaultVfoFreqHz = 14200000u;
  static constexpr uint32_t defaultVfoOffsetHz = 12000u;

  // Board-level channel and PLL assignments (Si5351C-B 20-pin QFN)
  static constexpr ClockChannel osd0Channel = ClockChannel::clk0;
  static constexpr ClockChannel osd1Channel = ClockChannel::clk3;
  static constexpr ClockChannel testGenChannel = ClockChannel::clk7;
  static constexpr Pll osd0Pll = Pll::pllA;
  static constexpr Pll osd1Pll = Pll::pllB;
  static constexpr DriveStrength defaultDriveStrength = DriveStrength::drive2mA;

  /**
   * @brief Initializes the Si5351C, routes CLKIN (/2) to both PLLs, sets the
   *        initial VFO frequencies on CLK0/CLK3, leaves CLK7 disabled, and
   *        enables outputs.
   * @return 0 on success, negative Zephyr errno on failure.
   */
  static int init(uint32_t initialFreqHz = defaultVfoFreqHz,
                  uint32_t initialOffsetHz = defaultVfoOffsetHz,
                  bool isFourPhase = false);

  /**
   * @brief Configures OSD0 (CLK0) and OSD1 (CLK3) synthesis frequencies.
   *        Avoids PLL resets unless required by DIVBY4 transition or lock loss.
   * @param freqHz Target VFO center frequency in Hz.
   * @param offsetHz Intermediate frequency offset in Hz (typically 12000).
   * @param isFourPhase True for 4-phase QSD (> 15 MHz), false for 8-phase OSD (< 15 MHz).
   * @return 0 on success, negative Zephyr errno on failure.
   */
  static int setVFOFrequency(uint32_t freqHz, uint32_t offsetHz, bool isFourPhase = false);

  /**
   * @brief Configures the CLK7 test generator using whichever running PLL
   *        produces the smallest output frequency error via MS7 and R7.
   * @param targetFreqHz Desired test frequency in Hz.
   * @param actualFreqHzOut Optional pointer receiving the achieved frequency in Hz.
   * @param enableOutput True to immediately power up and enable CLK7.
   * @return 0 on success, negative Zephyr errno on failure.
   */
  static int setTestFrequency(uint32_t targetFreqHz,
                              uint32_t* actualFreqHzOut = nullptr,
                              bool enableOutput = true);

  /**
   * @brief Enables or disables the CLK7 test generator output independently.
   * @return 0 on success, negative Zephyr errno on failure.
   */
  static int setTestGeneratorEnabled(bool enable);

  /**
   * @brief Sets the output drive strength for a specific clock channel.
   * @return 0 on success, negative Zephyr errno on failure.
   */
  static int setDriveStrength(ClockChannel channel, DriveStrength strength);

  /**
   * @brief Enables or disables an individual clock channel (CLKx_PDN and Reg 3).
   * @return 0 on success, negative Zephyr errno on failure.
   */
  static int setChannelEnabled(ClockChannel channel, bool enable);

  /**
   * @brief Globally enables or disables Si5351 clock outputs (Reg 3 and OEB pin).
   * @return 0 on success, negative Zephyr errno on failure.
   */
  static int outputEnable(bool enable);

  /**
   * @brief Updates the calibrated TCXO reference frequency in Hz (e.g., from
   *        GNSS 1PPS gated counting in the CPLD).
   * @param actualRefClockHz Measured TCXO frequency in Hz.
   * @param retuneActive True to immediately re-apply the active VFO frequency.
   * @return 0 on success, negative Zephyr errno on failure.
   */
  static int setRefClockHz(uint32_t actualRefClockHz, bool retuneActive = true);

  /**
   * @brief Updates the calibrated TCXO reference frequency in milliHz for
   *        multi-second GNSS 1PPS averaging.
   * @param actualRefClockMilliHz Measured TCXO frequency in mHz.
   * @param retuneActive True to immediately re-apply the active VFO frequency.
   * @return 0 on success, negative Zephyr errno on failure.
   */
  static int setRefClockMilliHz(uint64_t actualRefClockMilliHz, bool retuneActive = true);

  static uint32_t getRefClockHz();
  static uint64_t getRefClockMilliHz();

  static int readRegister(uint8_t reg, uint8_t& val);
  static int writeRegister(uint8_t reg, uint8_t val);
  static int writeRegisters(uint8_t startReg, const uint8_t* data, size_t len);
  static int readStatus(uint8_t& status);

private:
  static const struct device* getI2CDevice();
  static int waitForSysInit();
  static int waitForPllLock(uint8_t lolMask, uint32_t timeoutMs);
  static int resetPlls(uint8_t resetMask, uint8_t lolMask);
  static int ensurePllsLocked(uint8_t lolMask, uint8_t resetMask);
  static int configurePll(Pll pll, uint32_t pllFreqHz);
  static int configureMultisynth0To5(ClockChannel channel, uint32_t msDiv, uint8_t rDivExp);
  static int configureMultisynth6Or7(ClockChannel channel, uint32_t msDiv, uint8_t rDivExp);
  static int updateVfoChannel(ClockChannel channel,
                              Pll pll,
                              uint32_t outFreqHz,
                              uint8_t& forcedResetMask);
  static int updateTestGenerator(uint32_t* actualFreqHzOut);
};

} // namespace nexrx
