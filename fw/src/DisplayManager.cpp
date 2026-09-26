#include "DisplayManager.hpp"
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(nexrx_main, LOG_LEVEL_INF);

namespace nexrx {

const struct device* DisplayManager::displayDev = NULL;

void DisplayManager::init() {
  displayDev = DEVICE_DT_GET(DT_NODELABEL(st7789v));

  if (!device_is_ready(displayDev)) {
    LOG_ERR("Display: ST7789V not ready");
    return;
  }

  LOG_INF("Display: ST7789V initialized (240x135)");
  
  display_blanking_off(displayDev);
  clear();
  drawSplash();
}

void DisplayManager::clear() {
  /* TODO: Use display_write to fill screen with black */
  LOG_INF("Display: Screen cleared");
}

void DisplayManager::showStatus(const char* status) {
  /* TODO: Render text string to display */
  LOG_INF("Display Status: %s", status);
}

void DisplayManager::drawSplash() {
  /* TODO: Draw NexRx logo or splash screen */
  LOG_INF("Display: Splash screen drawn");
}

} // namespace nexrx
