#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <stdlib.h>
#include <string.h>

#include "CPLDDriver.hpp"
#include "Si5351Driver.hpp"
#include "LEDManager.hpp"

namespace nexrx {

namespace {

/* =========================================================================
 * CPLD Shell Commands
 * ========================================================================= */

int cmdCPLDStatus(const struct shell* sh, size_t argc, char** argv) {
  ARG_UNUSED(argc);
  ARG_UNUSED(argv);

  bool configured = CPLDDriver::isConfigured();
  shell_print(sh, "CPLD Configuration (DONE pin): %s", configured ? "ASSERTED (High)" : "NOT DONE (Low)");

  for (int i = 0; i < 2; ++i) {
    uint32_t sig = 0;
    int retSig = CPLDDriver::readRegister(CPLDDriver::regSig, sig, i);
    uint32_t counter = CPLDDriver::getTCXOCounter(i);

    if (retSig == 0) {
      char sigStr[5] = {
        static_cast<char>((sig >> 24) & 0xFF),
        static_cast<char>((sig >> 16) & 0xFF),
        static_cast<char>((sig >> 8) & 0xFF),
        static_cast<char>(sig & 0xFF),
        '\0'
      };
      shell_print(sh, "CPLD[%d]: Sig=0x%08X (\"%s\", %s), TCXO Counter=%u",
                  i, sig, sigStr,
                  (sig == CPLDDriver::expectedSig) ? "VALID" : "MISMATCH",
                  counter);
    } else {
      shell_error(sh, "CPLD[%d]: SPI read failed (%d)", i, retSig);
    }
  }
  return 0;
}

int cmdCPLDRead(const struct shell* sh, size_t argc, char** argv) {
  if (argc < 3) {
    shell_error(sh, "Usage: cpld read <index: 0|1> <addr: hex/dec>");
    return -EINVAL;
  }
  int index = atoi(argv[1]);
  if (index < 0 || index > 1) {
    shell_error(sh, "CPLD index must be 0 or 1");
    return -EINVAL;
  }
  uint8_t addr = static_cast<uint8_t>(strtoul(argv[2], NULL, 0));
  uint32_t val = 0;
  int ret = CPLDDriver::readRegister(addr, val, index);
  if (ret != 0) {
    shell_error(sh, "Failed to read CPLD[%d] register 0x%02X: err %d", index, addr, ret);
    return ret;
  }
  shell_print(sh, "CPLD[%d] Reg[0x%02X] = 0x%08X (%u)", index, addr, val, val);
  return 0;
}

int cmdCPLDWrite(const struct shell* sh, size_t argc, char** argv) {
  if (argc < 4) {
    shell_error(sh, "Usage: cpld write <index: 0|1|-1> <addr: hex/dec> <data: hex/dec>");
    return -EINVAL;
  }
  int index = atoi(argv[1]);
  uint8_t addr = static_cast<uint8_t>(strtoul(argv[2], NULL, 0));
  uint32_t val = static_cast<uint32_t>(strtoul(argv[3], NULL, 0));

  int ret = CPLDDriver::writeRegister(addr, val, index);
  if (ret != 0) {
    shell_error(sh, "Failed to write CPLD register 0x%02X: err %d", addr, ret);
    return ret;
  }
  shell_print(sh, "Wrote 0x%08X to CPLD[%d] Reg[0x%02X]", val, index, addr);
  return 0;
}

int cmdCPLDProg(const struct shell* sh, size_t argc, char** argv) {
  ARG_UNUSED(argc);
  ARG_UNUSED(argv);
  shell_print(sh, "Programming default embedded bitstream to dual CPLDs...");
  bool ok = CPLDDriver::programDefaultBitstream();
  if (ok) {
    shell_print(sh, "CPLD bitstream programmed and verified successfully.");
  } else {
    shell_error(sh, "CPLD programming failed!");
  }
  return ok ? 0 : -EIO;
}

int cmdCPLDMode(const struct shell* sh, size_t argc, char** argv) {
  if (argc < 2) {
    shell_error(sh, "Usage: cpld mode <4|8>");
    return -EINVAL;
  }
  int mode = atoi(argv[1]);
  if (mode == 4) {
    CPLDDriver::setClockMode(true);
    shell_print(sh, "CPLD Clock Mode set to 4-Phase QSD");
  } else if (mode == 8) {
    CPLDDriver::setClockMode(false);
    shell_print(sh, "CPLD Clock Mode set to 8-Phase OSD");
  } else {
    shell_error(sh, "Invalid mode: %d. Use 4 for 4-phase or 8 for 8-phase.", mode);
    return -EINVAL;
  }
  return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(cpldSubCmds,
  SHELL_CMD_ARG(status, NULL, "Show CPLD configuration, signatures, and counters", cmdCPLDStatus, 1, 0),
  SHELL_CMD_ARG(read,   NULL, "Read 32-bit register: cpld read <0|1> <addr>",     cmdCPLDRead,   3, 0),
  SHELL_CMD_ARG(write,  NULL, "Write 32-bit register: cpld write <0|1|-1> <addr> <val>", cmdCPLDWrite, 4, 0),
  SHELL_CMD_ARG(prog,   NULL, "Reprogram dual CPLDs with default embedded bitstream", cmdCPLDProg, 1, 0),
  SHELL_CMD_ARG(mode,   NULL, "Set CPLD clock mode: cpld mode <4|8>",             cmdCPLDMode,   2, 0),
  SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(cpld, &cpldSubCmds, "NexRx Dual CPLD SPI Commands", NULL);

/* =========================================================================
 * Si5351 Shell Commands
 * ========================================================================= */

int cmdSi5351Status(const struct shell* sh, size_t argc, char** argv) {
  ARG_UNUSED(argc);
  ARG_UNUSED(argv);

  uint8_t status = 0;
  int ret = Si5351Driver::readStatus(status);
  if (ret != 0) {
    shell_error(sh, "Failed to read Si5351 status: err %d", ret);
    return ret;
  }

  bool sysInit   = (status & 0x80) != 0;
  bool lolB      = (status & 0x40) != 0;
  bool lolA      = (status & 0x20) != 0;
  bool clkinLos  = (status & 0x10) != 0;
  uint8_t revId  = status & 0x03;

  shell_print(sh, "Si5351 Status (Reg 0): 0x%02X", status);
  shell_print(sh, "  SYS_INIT:  %s", sysInit  ? "1 (Initializing)" : "0 (Ready)");
  shell_print(sh, "  LOL_B:     %s", lolB     ? "1 (PLLB UNLOCKED)" : "0 (PLLB LOCKED)");
  shell_print(sh, "  LOL_A:     %s", lolA     ? "1 (PLLA UNLOCKED)" : "0 (PLLA LOCKED)");
  shell_print(sh, "  CLKIN_LOS: %s", clkinLos ? "1 (LOSS OF SIGNAL)" : "0 (40 MHz TCXO Present)");
  shell_print(sh, "  REV_ID:    %u", revId);
  return 0;
}

int cmdSi5351Read(const struct shell* sh, size_t argc, char** argv) {
  if (argc < 2) {
    shell_error(sh, "Usage: si5351 read <reg: dec/hex>");
    return -EINVAL;
  }
  uint8_t reg = static_cast<uint8_t>(strtoul(argv[1], NULL, 0));
  uint8_t val = 0;
  int ret = Si5351Driver::readRegister(reg, val);
  if (ret != 0) {
    shell_error(sh, "Failed to read Si5351 register %u: err %d", reg, ret);
    return ret;
  }
  shell_print(sh, "Si5351 Reg[%u / 0x%02X] = 0x%02X (%u)", reg, reg, val, val);
  return 0;
}

int cmdSi5351Write(const struct shell* sh, size_t argc, char** argv) {
  if (argc < 3) {
    shell_error(sh, "Usage: si5351 write <reg: dec/hex> <val: dec/hex>");
    return -EINVAL;
  }
  uint8_t reg = static_cast<uint8_t>(strtoul(argv[1], NULL, 0));
  uint8_t val = static_cast<uint8_t>(strtoul(argv[2], NULL, 0));
  int ret = Si5351Driver::writeRegister(reg, val);
  if (ret != 0) {
    shell_error(sh, "Failed to write Si5351 register %u: err %d", reg, ret);
    return ret;
  }
  shell_print(sh, "Wrote 0x%02X to Si5351 Reg[%u / 0x%02X]", val, reg, reg);
  return 0;
}

int cmdSi5351Tune(const struct shell* sh, size_t argc, char** argv) {
  if (argc < 2) {
    shell_error(sh, "Usage: si5351 tune <freqHz> [offsetHz: default 12000] [mode: 4|8]");
    return -EINVAL;
  }
  uint32_t freqHz = static_cast<uint32_t>(strtoul(argv[1], NULL, 0));
  uint32_t offsetHz = 12000;
  if (argc >= 3) {
    offsetHz = static_cast<uint32_t>(strtoul(argv[2], NULL, 0));
  }
  bool isFourPhase = (freqHz >= 15000000U);
  if (argc >= 4) {
    int m = atoi(argv[3]);
    isFourPhase = (m == 4);
  }

  bool ok = Si5351Driver::setVFOFrequency(freqHz, offsetHz, isFourPhase);
  if (ok) {
    shell_print(sh, "Si5351 tuned to %u Hz (IF offset: %u Hz, Mode: %s)",
                freqHz, offsetHz, isFourPhase ? "4-Phase" : "8-Phase");
  } else {
    shell_error(sh, "Si5351 tune failed!");
  }
  return ok ? 0 : -EIO;
}

int cmdSi5351OE(const struct shell* sh, size_t argc, char** argv) {
  if (argc < 2) {
    shell_error(sh, "Usage: si5351 oe <0|1>");
    return -EINVAL;
  }
  bool enable = (atoi(argv[1]) != 0);
  Si5351Driver::outputEnable(enable);
  shell_print(sh, "Si5351 Outputs %s", enable ? "Enabled" : "Disabled");
  return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(si5351SubCmds,
  SHELL_CMD_ARG(status, NULL, "Show Si5351 PLL lock and reference status", cmdSi5351Status, 1, 0),
  SHELL_CMD_ARG(read,   NULL, "Read Si5351 register: si5351 read <reg>",   cmdSi5351Read,   2, 0),
  SHELL_CMD_ARG(write,  NULL, "Write Si5351 register: si5351 write <reg> <val>", cmdSi5351Write, 3, 0),
  SHELL_CMD_ARG(tune,   NULL, "Tune VFO: si5351 tune <freqHz> [offsetHz] [4|8]", cmdSi5351Tune,  2, 2),
  SHELL_CMD_ARG(oe,     NULL, "Enable/disable outputs: si5351 oe <0|1>",   cmdSi5351OE,     2, 0),
  SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(si5351, &si5351SubCmds, "NexRx Si5351 Synthesizer Commands", NULL);

/* =========================================================================
 * LED Manager Shell Commands
 * ========================================================================= */

int cmdLEDStage(const struct shell* sh, size_t argc, char** argv) {
  if (argc < 2) {
    shell_print(sh, "Current stage: %s", LEDManager::instance().getCurrentStageName());
    shell_print(sh, "Available stages: poweron, cpld, si5351, usb, ready");
    return 0;
  }

  const char* stageStr = argv[1];
  if (strcmp(stageStr, "poweron") == 0) {
    LEDManager::instance().onPowerOn();
  } else if (strcmp(stageStr, "cpld") == 0) {
    LEDManager::instance().onCPLDConfig();
  } else if (strcmp(stageStr, "si5351") == 0) {
    LEDManager::instance().onSi5351Setup();
  } else if (strcmp(stageStr, "usb") == 0) {
    LEDManager::instance().onUSBConfig();
  } else if (strcmp(stageStr, "ready") == 0) {
    LEDManager::instance().onReady();
  } else {
    shell_error(sh, "Unknown stage '%s'. Use: poweron, cpld, si5351, usb, ready", stageStr);
    return -EINVAL;
  }
  shell_print(sh, "LED Stage set to %s", LEDManager::instance().getCurrentStageName());
  return 0;
}

int cmdLEDRGB(const struct shell* sh, size_t argc, char** argv) {
  if (argc < 4) {
    shell_error(sh, "Usage: led rgb <r: 0-255> <g: 0-255> <b: 0-255>");
    return -EINVAL;
  }
  uint8_t r = static_cast<uint8_t>(atoi(argv[1]));
  uint8_t g = static_cast<uint8_t>(atoi(argv[2]));
  uint8_t b = static_cast<uint8_t>(atoi(argv[3]));
  LEDManager::instance().setColor(r, g, b);
  shell_print(sh, "LED Breathing RGB hue set to (%u, %u, %u)", r, g, b);
  return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(ledSubCmds,
  SHELL_CMD_ARG(stage, NULL, "Get or set boot stage: led stage [poweron|cpld|si5351|usb|ready]", cmdLEDStage, 1, 1),
  SHELL_CMD_ARG(rgb,   NULL, "Set custom breathing RGB hue: led rgb <r> <g> <b>",                 cmdLEDRGB,   4, 0),
  SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(led, &ledSubCmds, "NexRx Status LED Breather Commands", NULL);

} // namespace

} // namespace nexrx
