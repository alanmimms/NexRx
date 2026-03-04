#include "ControlHandler.hpp"
#include <zephyr/logging/log.h>
#include "drivers/MAX9939.hpp"
#include "drivers/NexBus.hpp"
#include "drivers/FPGAManager.hpp"

LOG_MODULE_DECLARE(nexrx_main, LOG_LEVEL_INF);

namespace nexrx {

ControlHandler& ControlHandler::instance() {
  static ControlHandler inst;
  return inst;
}

void ControlHandler::process(ITransport& transport) {
  uint8_t buf[1024];
  int len = transport.receive(buf, sizeof(buf));
  
  if (len < 4) return;

  uint32_t cmdId = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];

  switch (cmdId) {
    case 0x5356464F: { /* "SVFO" - Set QSD VFO */
      LOG_INF("Control: Set VFO command received");
      /* TODO: Parse CBOR for lane and increment */
      /* Example: FPGAManager::setQSDIncrement(lane, inc); */
      /* FPGAManager::commitFrequencies(); */
      break;
    }

    case 0x53415454: { /* "SATT" - Set Attenuator */
      LOG_INF("Control: Set Attenuator command received");
      /* TODO: Parse CBOR and call NexBus */
      break;
    }

    case 0x53504741: { /* "SPGA" - Set PGA Gain */
      LOG_INF("Control: Set PGA Gain command received");
      /* TODO: Parse CBOR and call MAX9939::setGain */
      break;
    }

    case 0x4754494D: { /* "GTIM" - Get Timestamp */
      uint64_t ts = FPGAManager::getTCXOTimestamp();
      LOG_INF("Control: TCXO Timestamp = %llu", ts);
      /* TODO: Send back CBOR response */
      break;
    }

    default:
      LOG_WRN("Control: Unknown command ID 0x%08X", cmdId);
      break;
  }
}

} // namespace nexrx
