#include "USBManager.hpp"
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/buf.h>
#include "QSDCapture.hpp"

LOG_MODULE_DECLARE(nexrx_main, LOG_LEVEL_INF);

/* USB Device Next Instance - High Speed Composite */
USBD_DEVICE_DEFINE(nexrx_usb_dev, DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)), 
                   0x1209, 0x0001);

namespace nexrx {

struct usbd_context* USBManager::usbCtx = &nexrx_usb_dev;

/* 
 * Dedicated buffer pool for high-speed IQ data.
 * 4 buffers of 2048 bytes each to allow for double-buffering 
 * + transport overhead.
 */
NET_BUF_POOL_FIXED_DEFINE(iq_buf_pool, 4, 2048, 8, nullptr);

void USBManager::init() {
  int err = usbd_enable(usbCtx);
  if (err) {
    LOG_ERR("USB: Failed to enable stack (%d)", err);
    return;
  }
  LOG_INF("USB: Device Next Enabled (High-Speed Composite)");
}

int USBManager::submitBulkIn(uint8_t* data, size_t len) {
  struct net_buf* buf;

  /* Allocate buffer from pool (non-blocking) */
  buf = net_buf_alloc(&iq_buf_pool, K_NO_WAIT);
  if (!buf) {
    return -ENOMEM;
  }

  /* Copy data into net_buf (will be replaced by MDMA direct-to-pool later) */
  net_buf_add_mem(buf, data, len);

  /* 
   * Enqueue to Bulk IN endpoint (0x81).
   * In a full implementation, we would register a request completion 
   * handler. For now, we use the synchronous stub to maintain data flow.
   */
  int err = usbd_ep_enqueue(usbCtx, buf);
  if (err) {
    LOG_ERR("USB: EP Enqueue failed (%d)", err);
    net_buf_unref(buf);
    return err;
  }

  return 0;
}

/**
 * @brief Global completion handler for IQ data endpoint.
 * Called by the USB stack when the hardware is finished with the buffer.
 */
void USBManager::onBulkInComplete(struct usbd_context* udsCtx, 
                                  struct net_buf* buf, int err) {
  (void)udsCtx;
  if (err) {
    LOG_ERR("USB: Bulk IN transfer failed (%d)", err);
  }

  /* Free the buffer back to the pool */
  net_buf_unref(buf);

  /* Signal that the USB hardware is ready for more data */
  QSDCapture::usbBusy = false;
}

} // namespace nexrx
