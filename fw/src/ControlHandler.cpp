#include "ControlHandler.hpp"
#include "Control.hpp"
#include <zephyr/logging/log.h>
#include <zcbor_decode.h>
#include <zcbor_encode.h>
#include <cmath>

#include "TLV320ADC5140.hpp"
#include "Si5351Driver.hpp"
#include "CPLDDriver.hpp"
#include "AGCManager.hpp"

LOG_MODULE_DECLARE(nexrx_main, LOG_LEVEL_INF);

namespace nexrx {

static constexpr double freqBoundary15MHz = 15000000.0;

/* Active waterfall viewport state for hysteresis evaluation */
static double currentViewportMinHz = 14000000.0;
static double currentViewportMaxHz = 14350000.0;
static bool currentFourPhaseMode = false;

ControlHandler& ControlHandler::instance() {
  static ControlHandler inst;
  return inst;
}

static void updateClockModeHysteresis(double minHz, double maxHz) {
  currentViewportMinHz = minHz;
  currentViewportMaxHz = maxHz;

  if (currentViewportMinHz > freqBoundary15MHz) {
    if (!currentFourPhaseMode) {
      currentFourPhaseMode = true;
      CPLDDriver::setClockMode(true); /* Switch to 4-phase QSD mode */
      LOG_INF("Waterfall strictly above 15MHz: Switched CPLD to 4-phase QSD mode.");
    }
  } else if (currentViewportMaxHz < freqBoundary15MHz) {
    if (currentFourPhaseMode) {
      currentFourPhaseMode = false;
      CPLDDriver::setClockMode(false); /* Switch to 8-phase OSD mode */
      LOG_INF("Waterfall strictly below 15MHz: Switched CPLD to 8-phase OSD mode.");
    }
  } else {
    LOG_INF("Waterfall spans 15MHz boundary: Preserved active mode (%s).",
            currentFourPhaseMode ? "4-phase" : "8-phase");
  }
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
  zcbor_new_decode_state(state, 4, buf, len, 1, nullptr, 0);
  if (!zcbor_list_start_decode(state)) return;

  uint32_t cmdID = 0;
  if (!zcbor_uint32_decode(state, &cmdID)) return;

  bool handled = false;

  switch (cmdID) {
    case Control::CMD_SET_VFO: {
      double freq = 0.0, offset = 12000.0, spanHz = 384000.0;
      if (zcbor_float_decode(state, &freq)) {
        zcbor_float_decode(state, &offset);
        zcbor_float_decode(state, &spanHz);

        double minHz = freq - (spanHz / 2.0);
        double maxHz = freq + (spanHz / 2.0);
        updateClockModeHysteresis(minHz, maxHz);

        Si5351Driver::setVFOFrequency(static_cast<uint32_t>(freq), static_cast<uint32_t>(offset));
        handled = true;
      }
      break;
    }

    case Control::CMD_SET_ATTEN: {
      int32_t db = 0;
      if (zcbor_int32_decode(state, &db)) {
        LOG_INF("Control: Direct GPIO Attenuator set to %d dB", db);
        handled = true;
      }
      break;
    }

    case Control::CMD_SET_BPF_SELECT: {
      int32_t bpfIndex = 0;
      if (zcbor_int32_decode(state, &bpfIndex)) {
        LOG_INF("Control: Direct GPIO BPF select band %d", bpfIndex);
        handled = true;
      }
      break;
    }

    case Control::CMD_SET_PGA_GAIN: {
      int32_t code = 0;
      if (zcbor_int32_decode(state, &code)) {
        TLV320ADC5140::setPGAGain(static_cast<uint8_t>(code));
        handled = true;
      }
      break;
    }

    case Control::CMD_SET_AGC_MODE: {
      int32_t mode = 0;
      if (zcbor_int32_decode(state, &mode)) {
        AGCManager::setMode(static_cast<AGCManager::Mode>(mode));
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

  zcbor_list_end_decode(state);
}

} // namespace nexrx
