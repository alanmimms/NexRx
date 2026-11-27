#include "Si5351Driver.hpp"

#include <algorithm>
#include <errno.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(si5351, LOG_LEVEL_INF);

namespace nexrx {

namespace {

// Si5351C architectural counts and register map addresses (AN619)
constexpr int pllCount = 2;
constexpr int channelCount = 8;
constexpr int ms0To5Count = 6;

constexpr uint8_t regDeviceStatus = 0u;
constexpr uint8_t regOutputEnable = 3u;
constexpr uint8_t regPllInputSource = 15u;
constexpr uint8_t regClk0Control = 16u;
constexpr uint8_t regPllAParameters = 26u;
constexpr uint8_t regPllBParameters = 34u;
constexpr uint8_t regMs0Parameters = 42u;
constexpr uint8_t regMs6Parameters = 90u;
constexpr uint8_t regMs7Parameters = 91u;
constexpr uint8_t regR6R7Dividers = 92u;
constexpr uint8_t regPllReset = 177u;

constexpr uint8_t synthParamBytes = 8u;
constexpr uint8_t byteMask = static_cast<uint8_t>((1u << 8) - 1u);
constexpr uint8_t low2BitMask = static_cast<uint8_t>((1u << 2) - 1u);
constexpr uint8_t low4BitMask = static_cast<uint8_t>((1u << 4) - 1u);

// Register 0: Device Status bit masks
constexpr uint8_t statusSysInitMask = static_cast<uint8_t>(1u << 7);
constexpr uint8_t statusLolBMask = static_cast<uint8_t>(1u << 6);
constexpr uint8_t statusLolAMask = static_cast<uint8_t>(1u << 5);
constexpr uint8_t statusClkinLosMask = static_cast<uint8_t>(1u << 4);
constexpr uint8_t statusXoLosMask = static_cast<uint8_t>(1u << 3);

// Register 3: Output Enable Control (0 = enabled, 1 = disabled)
constexpr uint8_t allOutputsDisabledMask = static_cast<uint8_t>((1u << channelCount) - 1u);

// Register 15: PLL Input Source (Si5351C)
// Bits [7:6] = CLKIN_DIV (0: /1, 1: /2, 2: /4, 3: /8)
// Bit  3     = PLLB_SRC  (0: XTAL, 1: CLKIN)
// Bit  2     = PLLA_SRC  (0: XTAL, 1: CLKIN)
constexpr uint8_t pllSrcClkinDivShift = 6u;
constexpr uint8_t pllSrcClkinDivExp = 1u; // /2 -> 20 MHz PFD from 40 MHz TCXO
constexpr uint32_t clkinDivFactor = 1u << pllSrcClkinDivExp;
constexpr uint8_t pllSrcPllBClkinMask = static_cast<uint8_t>(1u << 3);
constexpr uint8_t pllSrcPllAClkinMask = static_cast<uint8_t>(1u << 2);
constexpr uint8_t pllInputSourceConfig = static_cast<uint8_t>(
  (pllSrcClkinDivExp << pllSrcClkinDivShift) |
  pllSrcPllBClkinMask |
  pllSrcPllAClkinMask
);

// Registers 16..23: CLKx_CONTROL bit definitions
// Note: Bit 6 is MSx_INT on CLK0..CLK5, but FBA_INT on CLK6 and FBB_INT on CLK7.
//       Never set bit 6 on CLK6/CLK7 while PLLA/PLLB run in fractional mode.
constexpr uint8_t clkCtrlPowerDownMask = static_cast<uint8_t>(1u << 7);
constexpr uint8_t clkCtrlMsIntModeMask = static_cast<uint8_t>(1u << 6);
constexpr uint8_t clkCtrlSrcPllBMask = static_cast<uint8_t>(1u << 5);
constexpr uint8_t clkCtrlInvertMask = static_cast<uint8_t>(1u << 4);
constexpr uint8_t clkCtrlSrcShift = 2u;
constexpr uint8_t clkCtrlSrcOwnMs = static_cast<uint8_t>(3u << clkCtrlSrcShift);
constexpr uint8_t clkCtrlDriveShift = 0u;
constexpr uint8_t clkCtrlDriveMask = static_cast<uint8_t>(3u << clkCtrlDriveShift);

// MultiSynth 0..5 byte 2 field positions
constexpr uint8_t msByte2RDivShift = 4u;
constexpr uint8_t msByte2DivBy4Shift = 2u;
constexpr uint8_t msByte2DivBy4Enable = static_cast<uint8_t>(3u << msByte2DivBy4Shift);

// Register 92: R6_DIV [2:0] and R7_DIV [6:4]
constexpr uint8_t r6DivShift = 0u;
constexpr uint8_t r7DivShift = 4u;
constexpr uint8_t rDivFieldMask = static_cast<uint8_t>((1u << 3) - 1u);
constexpr uint8_t rDivMaxExp = 7u; // /128

// Register 177: PLL Reset (self-clearing)
constexpr uint8_t pllBResetMask = static_cast<uint8_t>(1u << 7);
constexpr uint8_t pllAResetMask = static_cast<uint8_t>(1u << 5);

// PLL and MultiSynth hardware limits (AN619)
constexpr uint64_t milliHzPerHz = 1000uLL;
constexpr uint32_t pfdMinHz = 10000000u;
constexpr uint32_t pfdMaxHz = 40000000u;
constexpr uint32_t pllMinHz = 600000000u;
constexpr uint32_t pllMaxHz = 900000000u;
constexpr uint32_t pllMultMin = 15u;
constexpr uint32_t pllMultMax = 90u;
constexpr uint32_t pllFracDenomMax = (1u << 20) - 1u;

constexpr uint32_t synthP1Scale = 128u;
constexpr uint32_t synthP1Offset = 512u;

constexpr uint32_t msDivBy4 = 4u;
constexpr uint32_t ms0To5MinEvenDiv = 6u;
constexpr uint32_t ms0To5MaxEvenDiv = 2048u;
constexpr uint32_t ms6To7MinEvenDiv = 6u;
constexpr uint32_t ms6To7MaxEvenDiv = 254u;
constexpr uint32_t msDivBy4ThresholdHz = pllMaxHz / ms0To5MinEvenDiv; // 150 MHz
constexpr uint32_t msMaxOutputHz = 200000000u;
constexpr uint32_t msMinDirectOutHz = (pllMinHz + ms0To5MaxEvenDiv - 1u) / ms0To5MaxEvenDiv;

// Timing parameters
constexpr uint32_t sysInitTimeoutMs = 50u;
constexpr uint32_t lockSettleTimeoutMs = 5u;
constexpr uint32_t lockAfterResetTimeoutMs = 20u;

// OEB GPIO is marked GPIO_ACTIVE_LOW in DTS (logical 1 pulls pin 9 low -> enabled)
const struct gpio_dt_spec oeGpio = GPIO_DT_SPEC_GET_OR(DT_NODELABEL(si5351_OEB), oe_gpios, {0});

struct PllState {
  uint32_t freqHz = 0u;
  bool initialized = false;
  bool divBy4Active = false;
};

struct MsChannelState {
  uint32_t div = 0u;
  uint8_t rDivExp = 0u;
  bool configured = false;
};

// Runtime driver state
uint64_t refClockMilliHz = static_cast<uint64_t>(Si5351Driver::nominalRefClockHz) * milliHzPerHz;
PllState pllStates[pllCount];
MsChannelState msStates[ms0To5Count];
uint8_t clkControlCache[channelCount] = {};
uint8_t r6r7DivCache = 0u;
uint8_t channelEnabledMask = 0u;
bool globalOutputEnabled = false;

bool vfoConfigured = false;
uint32_t activeVfoFreqHz = 0u;
uint32_t activeVfoOffsetHz = 0u;
bool activeIsFourPhase = false;

bool testGenEnabled = false;
uint32_t testTargetFreqHz = 0u;

constexpr int toIndex(Si5351Driver::Pll pll) {
  return static_cast<int>(pll);
}

constexpr int toIndex(Si5351Driver::ClockChannel channel) {
  return static_cast<int>(channel);
}

constexpr uint8_t clkControlReg(Si5351Driver::ClockChannel channel) {
  return static_cast<uint8_t>(regClk0Control + static_cast<uint8_t>(channel));
}

constexpr uint8_t pllParamReg(Si5351Driver::Pll pll) {
  return (pll == Si5351Driver::Pll::pllA) ? regPllAParameters : regPllBParameters;
}

constexpr uint8_t ms0To5ParamReg(Si5351Driver::ClockChannel channel) {
  return static_cast<uint8_t>(regMs0Parameters + static_cast<uint8_t>(channel) * synthParamBytes);
}

constexpr uint8_t pllResetBit(Si5351Driver::Pll pll) {
  return (pll == Si5351Driver::Pll::pllA) ? pllAResetMask : pllBResetMask;
}

constexpr uint8_t pllLolBit(Si5351Driver::Pll pll) {
  return (pll == Si5351Driver::Pll::pllA) ? statusLolAMask : statusLolBMask;
}

constexpr uint8_t buildClkControl(Si5351Driver::Pll pll,
                                  Si5351Driver::DriveStrength drive,
                                  bool integerMsMode,
                                  bool poweredDown) {
  return static_cast<uint8_t>(
    (poweredDown ? clkCtrlPowerDownMask : 0u) |
    (integerMsMode ? clkCtrlMsIntModeMask : 0u) |
    ((pll == Si5351Driver::Pll::pllB) ? clkCtrlSrcPllBMask : 0u) |
    clkCtrlSrcOwnMs |
    ((static_cast<uint8_t>(drive) << clkCtrlDriveShift) & clkCtrlDriveMask)
  );
}

uint32_t gcd(uint32_t a, uint32_t b) {
  while (b != 0u) {
    uint32_t t = b;
    b = a % b;
    a = t;
  }
  return a;
}

void packSynthParams(uint32_t p1,
                     uint32_t p2,
                     uint32_t p3,
                     uint8_t byte2Flags,
                     uint8_t (&regData)[synthParamBytes]) {
  regData[0] = static_cast<uint8_t>((p3 >> 8) & byteMask);
  regData[1] = static_cast<uint8_t>(p3 & byteMask);
  regData[2] = static_cast<uint8_t>(((p1 >> 16) & low2BitMask) | byte2Flags);
  regData[3] = static_cast<uint8_t>((p1 >> 8) & byteMask);
  regData[4] = static_cast<uint8_t>(p1 & byteMask);
  regData[5] = static_cast<uint8_t>((((p3 >> 16) & low4BitMask) << 4) |
                                    ((p2 >> 16) & low4BitMask));
  regData[6] = static_cast<uint8_t>((p2 >> 8) & byteMask);
  regData[7] = static_cast<uint8_t>(p2 & byteMask);
}

int solveMs0To5(uint32_t outFreqHz,
                uint32_t& pllFreqHzOut,
                uint32_t& msDivOut,
                uint8_t& rDivExpOut) {
  if (outFreqHz == 0u || outFreqHz > msMaxOutputHz) {
    return -EINVAL;
  }

  if (outFreqHz > msDivBy4ThresholdHz) {
    uint32_t vcoHz = outFreqHz * msDivBy4;
    if (vcoHz < pllMinHz || vcoHz > pllMaxHz) {
      return -ERANGE;
    }
    pllFreqHzOut = vcoHz;
    msDivOut = msDivBy4;
    rDivExpOut = 0u;
    return 0;
  }

  uint8_t rExp = 0u;
  uint32_t msTargetHz = outFreqHz;
  while (msTargetHz < msMinDirectOutHz && rExp < rDivMaxExp) {
    ++rExp;
    msTargetHz = outFreqHz << rExp;
  }

  if (msTargetHz < msMinDirectOutHz) {
    return -ERANGE;
  }

  // Round down to an even divider from pllMaxHz so the VCO stays within [600, 900] MHz.
  uint32_t div = std::clamp((pllMaxHz / msTargetHz) & ~1u, ms0To5MinEvenDiv, ms0To5MaxEvenDiv);
  uint32_t vcoHz = msTargetHz * div;
  if (vcoHz < pllMinHz || vcoHz > pllMaxHz) {
    return -ERANGE;
  }

  pllFreqHzOut = vcoHz;
  msDivOut = div;
  rDivExpOut = rExp;
  return 0;
}

void logDeviceStatus(const char* context, uint8_t status) {
  LOG_INF("%s: Reg 0 = 0x%02X (SYS_INIT=%s, LOL_B=%s, LOL_A=%s, CLKIN_LOS=%s, XO_LOS=%s)",
          context,
          static_cast<unsigned int>(status),
          ((status & statusSysInitMask) != 0u) ? "init" : "ready",
          ((status & statusLolBMask) != 0u) ? "unlocked" : "locked",
          ((status & statusLolAMask) != 0u) ? "unlocked" : "locked",
          ((status & statusClkinLosMask) != 0u) ? "no-signal" : "valid",
          ((status & statusXoLosMask) != 0u) ? "no-signal" : "valid");
}

} // namespace

const struct device* Si5351Driver::getI2CDevice() {
  static const struct device* dev = DEVICE_DT_GET(DT_ALIAS(si5351_i2c));
  return dev;
}

int Si5351Driver::readRegister(uint8_t reg, uint8_t& val) {
  const struct device* dev = getI2CDevice();
  if (!device_is_ready(dev)) {
    return -ENODEV;
  }
  return i2c_write_read(dev, i2cAddress, &reg, 1u, &val, 1u);
}

int Si5351Driver::writeRegister(uint8_t reg, uint8_t val) {
  const struct device* dev = getI2CDevice();
  if (!device_is_ready(dev)) {
    return -ENODEV;
  }
  uint8_t buf[2] = {reg, val};
  return i2c_write(dev, buf, sizeof(buf), i2cAddress);
}

int Si5351Driver::writeRegisters(uint8_t startReg, const uint8_t* data, size_t len) {
  const struct device* dev = getI2CDevice();
  if (!device_is_ready(dev)) {
    return -ENODEV;
  }
  uint8_t buf[1u + synthParamBytes];
  if (data == nullptr || len == 0u || (len + 1u) > sizeof(buf)) {
    return -EINVAL;
  }
  buf[0] = startReg;
  for (int i = 0; i < static_cast<int>(len); ++i) {
    buf[i + 1] = data[i];
  }
  return i2c_write(dev, buf, len + 1u, i2cAddress);
}

int Si5351Driver::readStatus(uint8_t& status) {
  return readRegister(regDeviceStatus, status);
}

int Si5351Driver::waitForSysInit() {
  for (int elapsedMs = 0; elapsedMs < static_cast<int>(sysInitTimeoutMs); ++elapsedMs) {
    uint8_t status = 0u;
    int err = readStatus(status);
    if (err != 0) {
      return err;
    }
    if ((status & statusSysInitMask) == 0u) {
      return 0;
    }
    k_msleep(1);
  }
  LOG_ERR("Si5351: SYS_INIT did not clear within %u ms", static_cast<unsigned int>(sysInitTimeoutMs));
  return -ETIMEDOUT;
}

int Si5351Driver::waitForPllLock(uint8_t lolMask, uint32_t timeoutMs) {
  uint8_t status = 0u;
  for (int elapsedMs = 0; elapsedMs <= static_cast<int>(timeoutMs); ++elapsedMs) {
    int err = readStatus(status);
    if (err != 0) {
      return err;
    }
    if ((status & lolMask) == 0u) {
      return 0;
    }
    if (elapsedMs < static_cast<int>(timeoutMs)) {
      k_msleep(1);
    }
  }

  logDeviceStatus("Si5351 lock failure", status);
  if ((status & statusClkinLosMask) != 0u) {
    return -EIO;
  }
  return -ETIMEDOUT;
}

int Si5351Driver::resetPlls(uint8_t resetMask, uint8_t lolMask) {
  if (resetMask == 0u) {
    return 0;
  }
  int err = writeRegister(regPllReset, resetMask);
  if (err != 0) {
    return err;
  }
  return waitForPllLock(lolMask, lockAfterResetTimeoutMs);
}

int Si5351Driver::ensurePllsLocked(uint8_t lolMask, uint8_t resetMask) {
  uint8_t status = 0u;
  int err = readStatus(status);
  if (err != 0) {
    return err;
  }
  if ((status & lolMask) == 0u) {
    return 0;
  }

  // Allow normal VCO slewing to settle before resorting to a PLL reset.
  for (int elapsedMs = 0; elapsedMs < static_cast<int>(lockSettleTimeoutMs); ++elapsedMs) {
    k_msleep(1);
    err = readStatus(status);
    if (err != 0) {
      return err;
    }
    if ((status & lolMask) == 0u) {
      return 0;
    }
  }

  if ((status & statusClkinLosMask) != 0u) {
    logDeviceStatus("Si5351 CLKIN loss of signal", status);
    return -EIO;
  }

  uint8_t stuckResetMask = 0u;
  if ((status & statusLolAMask) != 0u && (resetMask & pllAResetMask) != 0u) {
    stuckResetMask |= pllAResetMask;
  }
  if ((status & statusLolBMask) != 0u && (resetMask & pllBResetMask) != 0u) {
    stuckResetMask |= pllBResetMask;
  }

  LOG_WRN("Si5351: PLL loss of lock detected (Reg 0 = 0x%02X); issuing targeted reset 0x%02X",
          static_cast<unsigned int>(status),
          static_cast<unsigned int>(stuckResetMask));
  return resetPlls(stuckResetMask, lolMask);
}

int Si5351Driver::init(uint32_t initialFreqHz, uint32_t initialOffsetHz, bool isFourPhase) {
  LOG_INF("Initializing Si5351C (ref=%u Hz, CLKIN_DIV=/%u)",
          static_cast<unsigned int>(getRefClockHz()),
          static_cast<unsigned int>(clkinDivFactor));

  if (device_is_ready(oeGpio.port)) {
    int err = gpio_pin_configure_dt(&oeGpio, GPIO_OUTPUT_INACTIVE);
    if (err != 0) {
      LOG_ERR("Si5351: Failed to configure OEB GPIO (%d)", err);
      return err;
    }
  }

  int err = waitForSysInit();
  if (err != 0) {
    return err;
  }

  // Reset cached runtime state
  for (int i = 0; i < pllCount; ++i) {
    pllStates[i] = PllState{};
  }
  for (int i = 0; i < ms0To5Count; ++i) {
    msStates[i] = MsChannelState{};
  }
  r6r7DivCache = 0u;
  channelEnabledMask = 0u;
  globalOutputEnabled = false;
  vfoConfigured = false;
  testGenEnabled = false;
  testTargetFreqHz = 0u;

  // Disable all outputs during setup
  err = writeRegister(regOutputEnable, allOutputsDisabledMask);
  if (err != 0) {
    return err;
  }

  // Power down all 8 clock outputs explicitly
  for (int ch = 0; ch < channelCount; ++ch) {
    clkControlCache[ch] = clkCtrlPowerDownMask;
    err = writeRegister(clkControlReg(static_cast<ClockChannel>(ch)), clkControlCache[ch]);
    if (err != 0) {
      return err;
    }
  }

  // Route CLKIN (/2 -> 20 MHz PFD) to both PLLA and PLLB
  err = writeRegister(regPllInputSource, pllInputSourceConfig);
  if (err != 0) {
    return err;
  }

  // Configure OSD0 (CLK0 -> PLLA) and OSD1 (CLK3 -> PLLB) in integer MultiSynth mode, 2 mA default
  clkControlCache[toIndex(osd0Channel)] =
    buildClkControl(osd0Pll, defaultDriveStrength, true, false);
  err = writeRegister(clkControlReg(osd0Channel), clkControlCache[toIndex(osd0Channel)]);
  if (err != 0) {
    return err;
  }

  clkControlCache[toIndex(osd1Channel)] =
    buildClkControl(osd1Pll, defaultDriveStrength, true, false);
  err = writeRegister(clkControlReg(osd1Channel), clkControlCache[toIndex(osd1Channel)]);
  if (err != 0) {
    return err;
  }

  // Prepare CLK7 cache (powered down by default; bit 6 MUST stay 0 because it is FBB_INT)
  clkControlCache[toIndex(testGenChannel)] =
    buildClkControl(Pll::pllA, defaultDriveStrength, false, true);
  err = writeRegister(clkControlReg(testGenChannel), clkControlCache[toIndex(testGenChannel)]);
  if (err != 0) {
    return err;
  }

  // Configure initial VFO frequencies (first-time init performs one PLL reset and waits for lock)
  err = setVFOFrequency(initialFreqHz, initialOffsetHz, isFourPhase);
  if (err != 0) {
    return err;
  }

  // Enable only OSD0 and OSD1; CLK7 stays disabled until requested for calibration/testing
  channelEnabledMask = static_cast<uint8_t>((1u << toIndex(osd0Channel)) |
                                            (1u << toIndex(osd1Channel)));
  err = outputEnable(true);
  if (err != 0) {
    return err;
  }

  uint8_t status = 0u;
  if (readStatus(status) == 0) {
    logDeviceStatus("Si5351C initialized", status);
  }
  return 0;
}

int Si5351Driver::configurePll(Pll pll, uint32_t pllFreqHz) {
  if (pllFreqHz < pllMinHz || pllFreqHz > pllMaxHz) {
    LOG_ERR("Si5351: Target PLL frequency %u Hz out of [%u, %u] Hz range",
            static_cast<unsigned int>(pllFreqHz),
            static_cast<unsigned int>(pllMinHz),
            static_cast<unsigned int>(pllMaxHz));
    return -ERANGE;
  }

  uint64_t pfdMilliHz = refClockMilliHz / clkinDivFactor;
  uint64_t pllMilliHz = static_cast<uint64_t>(pllFreqHz) * milliHzPerHz;

  uint32_t a = static_cast<uint32_t>(pllMilliHz / pfdMilliHz);
  uint64_t remainderMilliHz = pllMilliHz % pfdMilliHz;
  uint32_t c = pllFracDenomMax;
  uint64_t bRounded = (remainderMilliHz * c + (pfdMilliHz / 2uLL)) / pfdMilliHz;

  uint32_t b = 0u;
  if (bRounded >= c) {
    ++a;
    b = 0u;
    c = 1u;
  } else {
    b = static_cast<uint32_t>(bRounded);
    if (b == 0u) {
      c = 1u;
    } else {
      uint32_t divGcd = gcd(b, c);
      if (divGcd > 1u) {
        b /= divGcd;
        c /= divGcd;
      }
    }
  }

  if (a < pllMultMin || a > pllMultMax || (a == pllMultMax && b != 0u)) {
    LOG_ERR("Si5351: PLL feedback multiplier %u + %u/%u out of [%u, %u] range",
            static_cast<unsigned int>(a),
            static_cast<unsigned int>(b),
            static_cast<unsigned int>(c),
            static_cast<unsigned int>(pllMultMin),
            static_cast<unsigned int>(pllMultMax));
    return -ERANGE;
  }

  uint32_t fracTerm = (synthP1Scale * b) / c;
  uint32_t p1 = synthP1Scale * a + fracTerm - synthP1Offset;
  uint32_t p2 = synthP1Scale * b - c * fracTerm;
  uint32_t p3 = c;

  uint8_t regData[synthParamBytes];
  packSynthParams(p1, p2, p3, 0u, regData);
  return writeRegisters(pllParamReg(pll), regData, sizeof(regData));
}

int Si5351Driver::configureMultisynth0To5(ClockChannel channel, uint32_t msDiv, uint8_t rDivExp) {
  int chIdx = toIndex(channel);
  if (chIdx < 0 || chIdx >= ms0To5Count) {
    return -EINVAL;
  }
  bool validDiv = (msDiv == msDivBy4) ||
                  (msDiv >= ms0To5MinEvenDiv && msDiv <= ms0To5MaxEvenDiv && (msDiv & 1u) == 0u);
  if (!validDiv || rDivExp > rDivMaxExp) {
    LOG_ERR("Si5351: Invalid MS%d divider %u or R-div exp %u",
            chIdx,
            static_cast<unsigned int>(msDiv),
            static_cast<unsigned int>(rDivExp));
    return -EINVAL;
  }

  uint32_t p1 = (msDiv == msDivBy4) ? 0u : (synthP1Scale * msDiv - synthP1Offset);
  uint32_t p2 = 0u;
  uint32_t p3 = 1u;
  uint8_t byte2Flags = static_cast<uint8_t>(
    ((rDivExp & rDivFieldMask) << msByte2RDivShift) |
    ((msDiv == msDivBy4) ? msByte2DivBy4Enable : 0u)
  );

  uint8_t regData[synthParamBytes];
  packSynthParams(p1, p2, p3, byte2Flags, regData);
  return writeRegisters(ms0To5ParamReg(channel), regData, sizeof(regData));
}

int Si5351Driver::configureMultisynth6Or7(ClockChannel channel, uint32_t msDiv, uint8_t rDivExp) {
  if (channel != ClockChannel::clk6 && channel != ClockChannel::clk7) {
    return -EINVAL;
  }
  if (msDiv < ms6To7MinEvenDiv || msDiv > ms6To7MaxEvenDiv || (msDiv & 1u) != 0u ||
      rDivExp > rDivMaxExp) {
    return -EINVAL;
  }

  uint8_t msReg = (channel == ClockChannel::clk6) ? regMs6Parameters : regMs7Parameters;
  int err = writeRegister(msReg, static_cast<uint8_t>(msDiv));
  if (err != 0) {
    return err;
  }

  uint8_t rShift = (channel == ClockChannel::clk6) ? r6DivShift : r7DivShift;
  uint8_t rMask = static_cast<uint8_t>(rDivFieldMask << rShift);
  uint8_t nextR6R7 = static_cast<uint8_t>(
    (r6r7DivCache & static_cast<uint8_t>(~rMask)) |
    ((rDivExp & rDivFieldMask) << rShift)
  );
  if (nextR6R7 != r6r7DivCache) {
    err = writeRegister(regR6R7Dividers, nextR6R7);
    if (err != 0) {
      return err;
    }
    r6r7DivCache = nextR6R7;
  }
  return 0;
}

int Si5351Driver::updateVfoChannel(ClockChannel channel,
                                   Pll pll,
                                   uint32_t outFreqHz,
                                   uint8_t& forcedResetMask) {
  uint32_t targetPllHz = 0u;
  uint32_t targetDiv = 0u;
  uint8_t targetRDivExp = 0u;

  int err = solveMs0To5(outFreqHz, targetPllHz, targetDiv, targetRDivExp);
  if (err != 0) {
    return err;
  }

  PllState& pllState = pllStates[toIndex(pll)];
  MsChannelState& msState = msStates[toIndex(channel)];

  bool divBy4Next = (targetDiv == msDivBy4);
  if (!pllState.initialized || (divBy4Next != pllState.divBy4Active)) {
    forcedResetMask |= pllResetBit(pll);
  }

  bool msChanged = !msState.configured ||
                   (msState.div != targetDiv) ||
                   (msState.rDivExp != targetRDivExp);
  uint32_t oldTotalDiv = msState.configured ? (msState.div << msState.rDivExp) : 0u;
  uint32_t newTotalDiv = targetDiv << targetRDivExp;

  // Order MS vs PLL writes so divider transitions never momentarily overshoot outFreqHz.
  if (msChanged && (!msState.configured || newTotalDiv >= oldTotalDiv)) {
    err = configureMultisynth0To5(channel, targetDiv, targetRDivExp);
    if (err != 0) {
      return err;
    }
  }

  err = configurePll(pll, targetPllHz);
  if (err != 0) {
    return err;
  }

  if (msChanged && msState.configured && newTotalDiv < oldTotalDiv) {
    err = configureMultisynth0To5(channel, targetDiv, targetRDivExp);
    if (err != 0) {
      return err;
    }
  }

  pllState.freqHz = targetPllHz;
  pllState.initialized = true;
  pllState.divBy4Active = divBy4Next;

  msState.div = targetDiv;
  msState.rDivExp = targetRDivExp;
  msState.configured = true;
  return 0;
}

int Si5351Driver::setVFOFrequency(uint32_t freqHz, uint32_t offsetHz, bool isFourPhase) {
  if (freqHz <= offsetHz) {
    return -EINVAL;
  }

  uint32_t phaseMult = isFourPhase ? 4u : 8u;
  uint64_t clk0WideHz = static_cast<uint64_t>(freqHz - offsetHz) * phaseMult;
  uint64_t clk3WideHz = static_cast<uint64_t>(freqHz + offsetHz) * phaseMult;
  if (clk0WideHz > msMaxOutputHz || clk3WideHz > msMaxOutputHz) {
    return -ERANGE;
  }

  uint32_t clk0TargetHz = static_cast<uint32_t>(clk0WideHz);
  uint32_t clk3TargetHz = static_cast<uint32_t>(clk3WideHz);
  uint8_t forcedResetMask = 0u;

  int err = updateVfoChannel(osd0Channel, osd0Pll, clk0TargetHz, forcedResetMask);
  if (err != 0) {
    return err;
  }

  err = updateVfoChannel(osd1Channel, osd1Pll, clk3TargetHz, forcedResetMask);
  if (err != 0) {
    return err;
  }

  uint8_t bothLolMask = static_cast<uint8_t>(pllLolBit(osd0Pll) | pllLolBit(osd1Pll));
  uint8_t bothResetMask = static_cast<uint8_t>(pllResetBit(osd0Pll) | pllResetBit(osd1Pll));

  if (forcedResetMask != 0u) {
    err = resetPlls(forcedResetMask, bothLolMask);
  } else {
    err = ensurePllsLocked(bothLolMask, bothResetMask);
  }
  if (err != 0) {
    return err;
  }

  vfoConfigured = true;
  activeVfoFreqHz = freqHz;
  activeVfoOffsetHz = offsetHz;
  activeIsFourPhase = isFourPhase;

  LOG_INF("Si5351 Tune: Center=%u Hz (%s %ux) | CLK%d=%u Hz (PLLA=%u Hz, div=%u/%u), CLK%d=%u Hz (PLLB=%u Hz, div=%u/%u)",
          static_cast<unsigned int>(freqHz),
          isFourPhase ? "4-Phase" : "8-Phase",
          static_cast<unsigned int>(phaseMult),
          toIndex(osd0Channel),
          static_cast<unsigned int>(clk0TargetHz),
          static_cast<unsigned int>(pllStates[toIndex(osd0Pll)].freqHz),
          static_cast<unsigned int>(msStates[toIndex(osd0Channel)].div),
          static_cast<unsigned int>(1u << msStates[toIndex(osd0Channel)].rDivExp),
          toIndex(osd1Channel),
          static_cast<unsigned int>(clk3TargetHz),
          static_cast<unsigned int>(pllStates[toIndex(osd1Pll)].freqHz),
          static_cast<unsigned int>(msStates[toIndex(osd1Channel)].div),
          static_cast<unsigned int>(1u << msStates[toIndex(osd1Channel)].rDivExp));

  // If CLK7 test generator is active, keep it as close as possible to its target on the updated PLLs.
  if (testGenEnabled && testTargetFreqHz > 0u) {
    err = updateTestGenerator(nullptr);
    if (err != 0) {
      LOG_WRN("Si5351: Failed to track CLK7 test frequency after retune (%d)", err);
      return err;
    }
  }

  return 0;
}

int Si5351Driver::updateTestGenerator(uint32_t* actualFreqHzOut) {
  if (testTargetFreqHz == 0u) {
    return -EINVAL;
  }

  bool found = false;
  Pll bestPll = Pll::pllA;
  uint32_t bestMsDiv = ms6To7MinEvenDiv;
  uint8_t bestRDivExp = 0u;
  uint32_t bestActualHz = 0u;
  uint32_t bestErrorHz = static_cast<uint32_t>(~0u);

  for (int pllIdx = 0; pllIdx < pllCount; ++pllIdx) {
    if (!pllStates[pllIdx].initialized || pllStates[pllIdx].freqHz == 0u) {
      continue;
    }
    uint64_t pllHz = pllStates[pllIdx].freqHz;

    for (int rExp = 0; rExp <= static_cast<int>(rDivMaxExp); ++rExp) {
      uint64_t rDiv = 1uLL << rExp;
      uint64_t scaledTarget = static_cast<uint64_t>(testTargetFreqHz) * rDiv;
      uint64_t rawDiv = pllHz / scaledTarget;

      uint32_t candLow = static_cast<uint32_t>(
        std::clamp(rawDiv & ~1uLL,
                   static_cast<uint64_t>(ms6To7MinEvenDiv),
                   static_cast<uint64_t>(ms6To7MaxEvenDiv))
      );
      uint32_t candHigh = static_cast<uint32_t>(
        std::clamp((rawDiv + 2uLL) & ~1uLL,
                   static_cast<uint64_t>(ms6To7MinEvenDiv),
                   static_cast<uint64_t>(ms6To7MaxEvenDiv))
      );

      uint32_t candidates[2] = {candLow, candHigh};
      for (int cIdx = 0; cIdx < 2; ++cIdx) {
        uint32_t msDiv = candidates[cIdx];
        uint64_t totalDiv = static_cast<uint64_t>(msDiv) * rDiv;
        uint32_t actualHz = static_cast<uint32_t>((pllHz + (totalDiv / 2uLL)) / totalDiv);
        uint32_t errHz = (actualHz >= testTargetFreqHz)
                           ? (actualHz - testTargetFreqHz)
                           : (testTargetFreqHz - actualHz);

        if (!found || errHz < bestErrorHz) {
          found = true;
          bestPll = static_cast<Pll>(pllIdx);
          bestMsDiv = msDiv;
          bestRDivExp = static_cast<uint8_t>(rExp);
          bestActualHz = actualHz;
          bestErrorHz = errHz;
        }
      }
    }
  }

  if (!found) {
    return -EAGAIN;
  }

  int err = configureMultisynth6Or7(testGenChannel, bestMsDiv, bestRDivExp);
  if (err != 0) {
    return err;
  }

  // Update CLK7 PLL selection while preserving drive strength, power state, and FBB_INT = 0.
  int chIdx = toIndex(testGenChannel);
  uint8_t nextCtrl = static_cast<uint8_t>(
    (clkControlCache[chIdx] & static_cast<uint8_t>(~(clkCtrlSrcPllBMask | clkCtrlMsIntModeMask))) |
    ((bestPll == Pll::pllB) ? clkCtrlSrcPllBMask : 0u) |
    clkCtrlSrcOwnMs
  );
  if (nextCtrl != clkControlCache[chIdx]) {
    err = writeRegister(clkControlReg(testGenChannel), nextCtrl);
    if (err != 0) {
      return err;
    }
    clkControlCache[chIdx] = nextCtrl;
  }

  if (actualFreqHzOut != nullptr) {
    *actualFreqHzOut = bestActualHz;
  }

  LOG_INF("Si5351 CLK%d TestGen: target=%u Hz, actual=%u Hz (err=%u Hz, %s, MS7=%u, R7=/%u)",
          chIdx,
          static_cast<unsigned int>(testTargetFreqHz),
          static_cast<unsigned int>(bestActualHz),
          static_cast<unsigned int>(bestErrorHz),
          (bestPll == Pll::pllA) ? "PLLA" : "PLLB",
          static_cast<unsigned int>(bestMsDiv),
          static_cast<unsigned int>(1u << bestRDivExp));
  return 0;
}

int Si5351Driver::setTestFrequency(uint32_t targetFreqHz,
                                   uint32_t* actualFreqHzOut,
                                   bool enableOutput) {
  if (targetFreqHz == 0u) {
    return -EINVAL;
  }

  testTargetFreqHz = targetFreqHz;
  int err = updateTestGenerator(actualFreqHzOut);
  if (err != 0) {
    return err;
  }

  if (enableOutput) {
    err = setChannelEnabled(testGenChannel, true);
    if (err != 0) {
      return err;
    }
    testGenEnabled = true;
  }
  return 0;
}

int Si5351Driver::setTestGeneratorEnabled(bool enable) {
  if (enable) {
    if (testTargetFreqHz == 0u) {
      return -EINVAL;
    }
    int err = updateTestGenerator(nullptr);
    if (err != 0) {
      return err;
    }
    err = setChannelEnabled(testGenChannel, true);
    if (err != 0) {
      return err;
    }
    testGenEnabled = true;
    return 0;
  }

  testGenEnabled = false;
  return setChannelEnabled(testGenChannel, false);
}

int Si5351Driver::setDriveStrength(ClockChannel channel, DriveStrength strength) {
  int chIdx = toIndex(channel);
  if (chIdx < 0 || chIdx >= channelCount) {
    return -EINVAL;
  }

  uint8_t nextCtrl = static_cast<uint8_t>(
    (clkControlCache[chIdx] & static_cast<uint8_t>(~clkCtrlDriveMask)) |
    ((static_cast<uint8_t>(strength) << clkCtrlDriveShift) & clkCtrlDriveMask)
  );
  int err = writeRegister(clkControlReg(channel), nextCtrl);
  if (err != 0) {
    return err;
  }
  clkControlCache[chIdx] = nextCtrl;
  return 0;
}

int Si5351Driver::setChannelEnabled(ClockChannel channel, bool enable) {
  int chIdx = toIndex(channel);
  if (chIdx < 0 || chIdx >= channelCount) {
    return -EINVAL;
  }

  uint8_t chBit = static_cast<uint8_t>(1u << chIdx);
  if (enable) {
    // Power up the output stage before clearing its Reg 3 disable bit.
    uint8_t nextCtrl = static_cast<uint8_t>(
      clkControlCache[chIdx] & static_cast<uint8_t>(~clkCtrlPowerDownMask)
    );
    if (nextCtrl != clkControlCache[chIdx]) {
      int err = writeRegister(clkControlReg(channel), nextCtrl);
      if (err != 0) {
        return err;
      }
      clkControlCache[chIdx] = nextCtrl;
    }

    channelEnabledMask |= chBit;
    if (globalOutputEnabled) {
      return writeRegister(regOutputEnable, static_cast<uint8_t>(~channelEnabledMask));
    }
    return 0;
  }

  // Mask the output off in Reg 3 before powering down the output stage.
  channelEnabledMask &= static_cast<uint8_t>(~chBit);
  if (globalOutputEnabled) {
    int err = writeRegister(regOutputEnable, static_cast<uint8_t>(~channelEnabledMask));
    if (err != 0) {
      return err;
    }
  }

  uint8_t nextCtrl = static_cast<uint8_t>(clkControlCache[chIdx] | clkCtrlPowerDownMask);
  if (nextCtrl != clkControlCache[chIdx]) {
    int err = writeRegister(clkControlReg(channel), nextCtrl);
    if (err != 0) {
      return err;
    }
    clkControlCache[chIdx] = nextCtrl;
  }
  return 0;
}

int Si5351Driver::outputEnable(bool enable) {
  if (enable) {
    int err = writeRegister(regOutputEnable, static_cast<uint8_t>(~channelEnabledMask));
    if (err != 0) {
      return err;
    }
    if (device_is_ready(oeGpio.port)) {
      err = gpio_pin_set_dt(&oeGpio, 1);
      if (err != 0) {
        return err;
      }
    }
  } else {
    if (device_is_ready(oeGpio.port)) {
      int err = gpio_pin_set_dt(&oeGpio, 0);
      if (err != 0) {
        return err;
      }
    }
    int err = writeRegister(regOutputEnable, allOutputsDisabledMask);
    if (err != 0) {
      return err;
    }
  }

  globalOutputEnabled = enable;
  LOG_INF("Si5351 Outputs %s (mask=0x%02X)",
          enable ? "ENABLED" : "DISABLED",
          static_cast<unsigned int>(channelEnabledMask));
  return 0;
}

int Si5351Driver::setRefClockHz(uint32_t actualRefClockHz, bool retuneActive) {
  return setRefClockMilliHz(static_cast<uint64_t>(actualRefClockHz) * milliHzPerHz, retuneActive);
}

int Si5351Driver::setRefClockMilliHz(uint64_t actualRefClockMilliHz, bool retuneActive) {
  uint64_t pfdMilliHz = actualRefClockMilliHz / clkinDivFactor;
  uint64_t minPfdMilliHz = static_cast<uint64_t>(pfdMinHz) * milliHzPerHz;
  uint64_t maxPfdMilliHz = static_cast<uint64_t>(pfdMaxHz) * milliHzPerHz;
  if (pfdMilliHz < minPfdMilliHz || pfdMilliHz > maxPfdMilliHz) {
    return -ERANGE;
  }

  refClockMilliHz = actualRefClockMilliHz;
  if (retuneActive && vfoConfigured) {
    return setVFOFrequency(activeVfoFreqHz, activeVfoOffsetHz, activeIsFourPhase);
  }
  return 0;
}

uint32_t Si5351Driver::getRefClockHz() {
  return static_cast<uint32_t>((refClockMilliHz + (milliHzPerHz / 2uLL)) / milliHzPerHz);
}

uint64_t Si5351Driver::getRefClockMilliHz() {
  return refClockMilliHz;
}

} // namespace nexrx
