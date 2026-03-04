#include "USBManager.hpp"
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(nexrx_main, LOG_LEVEL_INF);

/* USB Device Next Instance - High Speed Composite */
USBD_DEVICE_DEFINE(nexrx_usb_dev, DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)), 
                   0x1209, 0x0001);

namespace nexrx {

void USBManager::init() {
  int err = usbd_enable(&nexrx_usb_dev);
  if (err) {
    LOG_ERR("Failed to enable USB Device Next (%d)", err);
    return;
  }
  LOG_INF("USB Device Next Enabled (High-Speed Composite)");
}

} // namespace nexrx
