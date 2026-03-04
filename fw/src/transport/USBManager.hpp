#pragma once

#include <zephyr/kernel.h>
#include <zephyr/usb/usbd.h>

namespace nexrx {

class USBManager {
public:
  static void init();
};

} // namespace nexrx
