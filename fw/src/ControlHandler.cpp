#include "ControlHandler.hpp"
#include "Control.hpp"
#include <zephyr/logging/log.h>
#include <zcbor_decode.h>
#include <zcbor_encode.h>
#include <cmath>

#include "TLV320ADC5140.hpp"
#include "Si5351Driver.hpp"
#include "CPLDDriver.hpp"
#include "FrontEndDriver.hpp"
#include "OSDCapture.hpp"
#include "DisplayManager.hpp"
#include "AGCManager.hpp"

LOG_MODULE_DECLARE(nexrx_main, LOG_LEVEL_INF);

namespace nexrx {

static constexpr double freqBoundary15MHz = 15000000.0;

/* Active waterfall viewport state for hysteresis evaluation */
static double currentViewportMinHz = 14000000.0;
static double currentViewportMaxHz = 14350000.0;
static bool currentFourPhaseMode = false;
static uint32_t currentVFOFreqHz = 14200000;
static uint32_t currentVFOOffsetHz = 12000;

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
      OSDCapture::setFourPhaseMode(true);
      LOG_INF("Waterfall strictly above 15MHz: Switched CPLD & DSP to 4-phase QSD mode.");
    }
  } else if (currentViewportMaxHz < freqBoundary15MHz) {
    if (currentFourPhaseMode) {
      currentFourPhaseMode = false;
      CPLDDriver::setClockMode(false); /* Switch to 8-phase OSD mode */
      OSDCapture::setFourPhaseMode(false);
      LOG_INF("Waterfall strictly below 15MHz: Switched CPLD & DSP to 8-phase OSD mode.");
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
  zcbor_list_start_encode(encState, 10);
  zcbor_int32_put(encState, 0);
  zcbor_list_end_encode(encState, 10);
  transport.send(outBuf, encState->payload - outBuf);
}

static void sendVersionResponse(ITransport& transport) {
  uint8_t outBuf[64];
  zcbor_state_t encState[2];
  zcbor_new_encode_state(encState, 2, outBuf, sizeof(outBuf), 1);

  uint32_t hwState = CPLDDriver::isConfigured() ? Control::STATE_READY : Control::STATE_WAIT_CPLD_IMAGE;

  zcbor_list_start_encode(encState, 10);
  zcbor_int32_put(encState, 0); // Status OK
  zcbor_uint32_put(encState, Control::FW_PROTOCOL_MAGIC);
  zcbor_uint32_put(encState, Control::FW_VERSION_MAJOR);
  zcbor_uint32_put(encState, Control::FW_VERSION_MINOR);
  zcbor_uint32_put(encState, Control::FW_VERSION_PATCH);
  zcbor_uint32_put(encState, hwState);
  zcbor_list_end_encode(encState, 10);

  transport.send(outBuf, encState->payload - outBuf);
}

static void sendStateResponse(ITransport& transport) {
  uint8_t outBuf[64];
  zcbor_state_t encState[2];
  zcbor_new_encode_state(encState, 2, outBuf, sizeof(outBuf), 1);

  uint32_t hwState = CPLDDriver::isConfigured() ? Control::STATE_READY : Control::STATE_WAIT_CPLD_IMAGE;
  uint64_t tcxoCount = CPLDDriver::getLatchedTCXOCount(0);

  zcbor_list_start_encode(encState, 10);
  zcbor_int32_put(encState, 0); // Status OK
  zcbor_uint32_put(encState, hwState);
  zcbor_int32_put(encState, FrontEndDriver::getAttenuation());
  zcbor_int32_put(encState, FrontEndDriver::getBpfIndex());
  zcbor_bool_put(encState, FrontEndDriver::isHpfBypassed());
  zcbor_bool_put(encState, currentFourPhaseMode);
  zcbor_uint64_put(encState, tcxoCount);
  zcbor_list_end_encode(encState, 10);

  transport.send(outBuf, encState->payload - outBuf);
}

void ControlHandler::process(ITransport& transport) {
  static uint8_t buf[8192]; // Supports full ~7.1 KB CPLD bitstream in single CBOR payload
  int len = transport.receive(buf, sizeof(buf));
  if (len < 1) return;

  zcbor_state_t state[4];
  zcbor_new_decode_state(state, 4, buf, len, 1, nullptr, 0);
  if (!zcbor_list_start_decode(state)) return;

  uint32_t cmdID = 0;
  if (!zcbor_uint32_decode(state, &cmdID)) return;

  bool handled = false;

  switch (cmdID) {
    case Control::CMD_GET_VERSION: {
      sendVersionResponse(transport);
      handled = false; // Response already sent
      break;
    }

    case Control::CMD_GET_STATE: {
      sendStateResponse(transport);
      handled = false; // Response already sent
      break;
    }

    case Control::CMD_LOAD_CPLD: {
      struct zcbor_string bitstreamPayload;
      if (zcbor_bstr_decode(state, &bitstreamPayload)) {
        LOG_INF("Control: Received CPLD Bitstream (%u bytes). Programming...",
                static_cast<unsigned int>(bitstreamPayload.len));
        bool ok = CPLDDriver::programBitstream(bitstreamPayload.value, bitstreamPayload.len);
        if (ok) {
          DisplayManager::showStatus("READY");
          handled = true;
        } else {
          DisplayManager::showStatus("CPLD ERR");
          LOG_ERR("Control: CPLD Programming Failed!");
        }
      }
      break;
    }

    case Control::CMD_START_STREAM: {
      OSDCapture::start();
      LOG_INF("Control: Streaming started.");
      handled = true;
      break;
    }

    case Control::CMD_STOP_STREAM: {
      OSDCapture::stop();
      LOG_INF("Control: Streaming stopped.");
      handled = true;
      break;
    }

    case Control::CMD_SET_VFO: {
      double freq = 0.0, offset = 12000.0, spanHz = 384000.0;
      if (zcbor_float_decode(state, &freq)) {
        zcbor_float_decode(state, &offset);
        zcbor_float_decode(state, &spanHz);

        currentVFOFreqHz = static_cast<uint32_t>(freq);
        currentVFOOffsetHz = static_cast<uint32_t>(offset);

        double minHz = freq - (spanHz / 2.0);
        double maxHz = freq + (spanHz / 2.0);
        updateClockModeHysteresis(minHz, maxHz);

        FrontEndDriver::selectFilterBand(currentVFOFreqHz);
        Si5351Driver::setVFOFrequency(currentVFOFreqHz, currentVFOOffsetHz, currentFourPhaseMode);
        handled = true;
      }
      break;
    }

    case Control::CMD_SET_ATTEN: {
      int32_t db = 0;
      if (zcbor_int32_decode(state, &db)) {
        FrontEndDriver::setAttenuation(db);
        handled = true;
      }
      break;
    }

    case Control::CMD_SET_BPF_SELECT: {
      int32_t bpfIndex = 0;
      if (zcbor_int32_decode(state, &bpfIndex)) {
        FrontEndDriver::setBpfIndex(bpfIndex);
        handled = true;
      }
      break;
    }

    case Control::CMD_SET_HPF_BYPASS: {
      bool bypass = false;
      if (zcbor_bool_decode(state, &bypass)) {
        FrontEndDriver::setHpfBypass(bypass);
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
