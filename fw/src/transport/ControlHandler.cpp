#include "ControlHandler.hpp"
#include "Control.hpp"
#include <zephyr/logging/log.h>
#include <zcbor_decode.h>
#include <zcbor_encode.h>
#include <cmath>

#include "../drivers/MAX9939.hpp"
#include "../drivers/NexBus.hpp"
#include "../drivers/FPGAManager.hpp"
#include "../app/AGCManager.hpp"

LOG_MODULE_DECLARE(nexrx_main, LOG_LEVEL_INF);

namespace nexrx {

static constexpr double fSys = 180000000.0;
static constexpr double pow2Value32 = 4294967296.0;

/* Cache target state to allow incremental bit updates */
static uint8_t targets[4] = {0, 0, 0, 0};
static bool preselEnabled = true;

ControlHandler& ControlHandler::instance() {
  static ControlHandler inst;
  return inst;
}

static uint32_t calculateIncrement(double freqHz, int multiplier) {
  if (freqHz <= 0.0) return 0;
  if (freqHz == 1.0) return 1;
  double target = freqHz * static_cast<double>(multiplier);
  double inc = (target / fSys) * pow2Value32;
  return static_cast<uint32_t>(std::floor(inc));
}

static void sendSuccessResponse(ITransport& transport) {
  uint8_t outBuf[16];
  zcbor_state_t encState[2];
  zcbor_new_encode_state(encState, 2, outBuf, sizeof(outBuf), 1);
  zcbor_int32_put(encState, 0);
  transport.send(outBuf, encState->payload - outBuf);
}

void ControlHandler::process(ITransport& transport) {
  uint8_t buf[1024];
  int len = transport.receive(buf, sizeof(buf));
  if (len < 1) return;

  zcbor_state_t state[4];
  zcbor_new_decode_state(state, 4, buf, len, 1);
  if (!zcbor_array_start_decode(state)) return;

  uint32_t cmdID = 0;
  if (!zcbor_uint32_decode(state, &cmdID)) return;

  bool handled = false;

  switch (cmdID) {
    case Control::CMD_SET_VFO: {
      double freq, offset;
      if (zcbor_float_decode(state, &freq) && zcbor_float_decode(state, &offset)) {
        FPGAManager::setQSDIncrement(0, calculateIncrement(freq - offset, 4));
        FPGAManager::setQSDIncrement(1, calculateIncrement(freq + offset, 4));
        FPGAManager::setQSDIncrement(2, calculateIncrement(freq, 6));
        FPGAManager::commit(true, false);
        handled = true;
      }
      break;
    }

    case Control::CMD_SET_ATTEN: {
      int32_t db;
      if (zcbor_int32_decode(state, &db)) {
        targets[3] &= 0xF0; /* Keep high bits (txrx) */
        if (db >= 24) { targets[3] |= (1 << 0); db -= 24; }
        if (db >= 12) { targets[3] |= (1 << 1); db -= 12; }
        if (db >= 6)  { targets[3] |= (1 << 2); db -= 6;  }
        if (db >= 3)  { targets[3] |= (1 << 3); db -= 3;  }
        NexBus::setTarget(3, targets[3]);
        NexBus::commit();
        handled = true;
      }
      break;
    }

    case Control::CMD_SET_PRESEL_L: {
      int32_t idx;
      bool en;
      if (zcbor_int32_decode(state, &idx) && zcbor_bool_decode(state, &en)) {
        if (idx == 1) {
          if (en) targets[2] |= (1 << 3); else targets[2] &= ~(1 << 3);
          if (preselEnabled) NexBus::setTarget(2, targets[2]);
          NexBus::commit();
        }
        handled = true;
      }
      break;
    }

    case Control::CMD_SET_PRESEL_C: {
      int32_t idx;
      bool en;
      if (zcbor_int32_decode(state, &idx) && zcbor_bool_decode(state, &en)) {
        if (idx >= 0 && idx <= 3) {
          if (en) targets[0] |= (1 << idx); else targets[0] &= ~(1 << idx);
          if (preselEnabled) NexBus::setTarget(0, targets[0]);
        } else if (idx >= 4 && idx <= 7) {
          if (en) targets[1] |= (1 << (idx - 4)); else targets[1] &= ~(1 << (idx - 4));
          if (preselEnabled) NexBus::setTarget(1, targets[1]);
        } else if (idx >= 8 && idx <= 10) {
          if (en) targets[2] |= (1 << (idx - 8)); else targets[2] &= ~(1 << (idx - 8));
          if (preselEnabled) NexBus::setTarget(2, targets[2]);
        }
        NexBus::commit();
        handled = true;
      }
      break;
    }

    case Control::CMD_SET_PRESEL_EN: {
      bool en;
      if (zcbor_bool_decode(state, &en)) {
        preselEnabled = en;
        if (preselEnabled) {
          for (int i = 0; i < 3; ++i) NexBus::setTarget(i, targets[i]);
        } else {
          for (int i = 0; i < 3; ++i) NexBus::setTarget(i, 0);
        }
        NexBus::commit();
        handled = true;
      }
      break;
    }

    case Control::CMD_SET_PGA_GAIN: {
      int32_t code;
      if (zcbor_int32_decode(state, &code)) {
        MAX9939::setGain(static_cast<uint8_t>(code));
        handled = true;
      }
      break;
    }

    case Control::CMD_SET_AGC_MODE: {
      int32_t mode;
      if (zcbor_int32_decode(state, &mode)) {
        AGCManager::setMode(static_cast<AGCManager::Mode>(mode));
        handled = true;
      }
      break;
    }

    case Control::CMD_SET_TR_MODE: {
      int32_t mode;
      if (zcbor_int32_decode(state, &mode)) {
        /* txrx is bit 4 in our target mapping (PA8/PB7) */
        if (mode == 1) {
          for (int i = 0; i < 4; ++i) targets[i] |= (1 << 4);
        } else {
          for (int i = 0; i < 4; ++i) targets[i] &= ~(1 << 4);
        }
        for (int i = 0; i < 4; ++i) {
          if (i == 3 || preselEnabled) NexBus::setTarget(i, targets[i]);
        }
        NexBus::commit();
        handled = true;
      }
      break;
    }

    default:
      LOG_WRN("Control: Unknown command 0x%08X", cmdID);
      break;
  }

  if (handled) {
    sendSuccessResponse(transport);
  }
  
  zcbor_array_end_decode(state);
}

} // namespace nexrx
