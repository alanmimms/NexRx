#include "AK5578.hpp"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(AK5578, LOG_LEVEL_INF);

namespace nexrx {

void AK5578::init() {
  LOG_INF("AK5578 8-Channel 24-bit 96kS/s Audio Codec initialized.");
}

void AK5578::reset() {
  LOG_INF("AK5578 reset performed.");
}

} // namespace nexrx
