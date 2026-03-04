#include "ControlHandler.hpp"
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(nexrx_main, LOG_LEVEL_INF);

namespace nexrx {

ControlHandler& ControlHandler::instance() {
  static ControlHandler inst;
  return inst;
}

void ControlHandler::process(ITransport& transport) {
  uint8_t buf[1024];
  int len = transport.receive(buf, sizeof(buf));
  if (len > 0) {
    LOG_INF("Control: Received command (%d bytes)", len);
  }
}

} // namespace nexrx
