#pragma once

#include <zephyr/kernel.h>
#include <stdint.h>

namespace nexrx {

class MAX9939 {
public:
  static void init();
  static void setGain(uint8_t code);
};

} // namespace nexrx
