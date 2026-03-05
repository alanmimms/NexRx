#include "ControlHandler.hpp"
#include <zephyr/logging/log.h>
#include <zcbor_decode.h>
#include <cmath>

#include "../drivers/MAX9939.hpp"
#include "../drivers/NexBus.hpp"
#include "../drivers/FPGAManager.hpp"
#include "../app/AGCManager.hpp"

LOG_MODULE_DECLARE(nexrx_main, LOG_LEVEL_INF);

namespace nexrx {

static constexpr double fSys = 180000000.0;
static constexpr double pow2Value32 = 4294967296.0;

ControlHandler& ControlHandler::instance() {
  static ControlHandler inst;
  return inst;
}

static uint32_t calculateIncrement(double freqHz, int multiplier) {
  if (freqHz <= 0.0) {
    return 0;
  }
  if (freqHz == 1.0) {
    return 1;
  }
  double target = freqHz * static_cast<double>(multiplier);
  double inc = (target / fSys) * pow2Value32;
  return static_cast<uint32_t>(std::floor(inc));
}

void ControlHandler::process(ITransport& transport) {
  uint8_t buf[1024];
  int len = transport.receive(buf, sizeof(buf));
  if (len < 1) {
    return;
  }

  zcbor_state_t state[4];
  zcbor_new_decode_state(state, 4, buf, len, 1);
  if (!zcbor_array_start_decode(state)) {
    return;
  }

  struct zcbor_string cmdStr;
  if (!zcbor_tstr_decode(state, &cmdStr)) {
    return;
  }

  uint32_t cmdID = 0;
  if (cmdStr.len >= 4) {
    cmdID = (cmdStr.value[0] << 24) | (cmdStr.value[1] << 16) | 
            (cmdStr.value[2] << 8) | cmdStr.value[3];
  }

  switch (cmdID) {
    case 0x5356464F: { /* "SVFO" */
      double freq, offset;
      if (zcbor_float_decode(state, &freq) && zcbor_float_decode(state, &offset)) {
        FPGAManager::setQSDIncrement(0, calculateIncrement(freq - offset, 4));
        FPGAManager::setQSDIncrement(1, calculateIncrement(freq + offset, 4));
        FPGAManager::setQSDIncrement(2, calculateIncrement(freq, 6));
        FPGAManager::commit(true, false);
      }
      break;
    }

    case 0x53415454: { /* "SATT" - Direct Attenuator Control */
      int32_t db;
      if (zcbor_int32_decode(state, &db)) {
        uint32_t mask = 0;
        if (db >= 24) { mask |= (1 << 3); db -= 24; }
        if (db >= 12) { mask |= (1 << 2); db -= 12; }
        if (db >= 6)  { mask |= (1 << 1); db -= 6;  }
        if (db >= 3)  { mask |= (1 << 0); db -= 3;  }
        NexBus::transmit(mask, 32);
      }
      break;
    }

    case 0x53504741: { /* "SPGA" - Direct PGA Control */
      int32_t code;
      if (zcbor_int32_decode(state, &code)) {
        MAX9939::setGain(static_cast<uint8_t>(code));
      }
      break;
    }

    case 0x53414743: { /* "SAGC" - Set AGC Mode */
      int32_t mode;
      if (zcbor_int32_decode(state, &mode)) {
        AGCManager::setMode(static_cast<AGCManager::Mode>(mode));
      }
      break;
    }

    default:
      break;
  }
  zcbor_array_end_decode(state);
}

} // namespace nexrx
