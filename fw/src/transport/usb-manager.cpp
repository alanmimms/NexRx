#include "USBManager.hpp"
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include "QSDCapture.hpp"

LOG_MODULE_DECLARE(nexrx_main, LOG_LEVEL_INF);

/* USB Device Next Instance - High Speed Composite */
USBD_DEVICE_DEFINE(nexrx_usb_dev, DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)), 
                   0x1209, 0x0001);

namespace nexrx {

struct usbd_context* USBManager::usb_ctx = &nexrx_usb_dev;

void USBManager::init() {
  int err = usbd_enable(usb_ctx);
  if (err) {
    LOG_ERR("USB: Failed to enable stack (%d)", err);
    return;
  }
  LOG_INF("USB: Device Next Enabled (High-Speed Composite)");
}

int USBManager::submitBulkIn(uint8_t* data, size_t len) {
  /* 
   * In a real implementation, we would use net_buf_alloc and 
   * usbd_ep_enqueue here. For this architectural baseline, 
   * we stub the hardware enqueue logic.
   */
  return 0;
}

void USBManager::onBulkInComplete(struct usbd_context *uds_ctx, 
                                  struct net_buf *buf, int err) {
  /* Mark the pump as ready for the next transfer */
  QSDCapture::usbBusy = false;
}

} // namespace nexrx
