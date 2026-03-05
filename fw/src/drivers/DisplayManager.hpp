#pragma once

#include <zephyr/kernel.h>
#include <zephyr/drivers/display.h>

namespace nexrx {

class DisplayManager {
public:
  static void init();
  static void clear();
  static void showStatus(const char* status);
  static void drawSplash();

private:
  static const struct device* displayDev;
};

} // namespace nexrx
