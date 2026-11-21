#include "USBManager.hpp"
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/net_buf.h>
#include <zephyr/usb/usbd.h>
#include "OSDCapture.hpp"

LOG_MODULE_DECLARE(nexrx_main, LOG_LEVEL_INF);

/* USB Device Next Instance - High Speed Composite - StarviewLabs NexRx */
USBD_DEVICE_DEFINE(nexrxUsbDev, DEVICE_DT_GET(DT_CHOSEN(zephyr_udc0)),
                   nexrx::USBManager::vendor, nexrx::USBManager::device);

USBD_DESC_LANG_DEFINE(nexrxLang);
USBD_DESC_MANUFACTURER_DEFINE(nexrxMfg, "Starview Labs");
USBD_DESC_PRODUCT_DEFINE(nexrxProduct, "NexRx SDR");

USBD_CONFIGURATION_DEFINE(nexrxHsConfig, USB_SCD_SELF_POWERED, 100, nullptr);
USBD_CONFIGURATION_DEFINE(nexrxFsConfig, USB_SCD_SELF_POWERED, 100, nullptr);

namespace nexrx {

struct usbd_context* USBManager::usbCtx = &nexrxUsbDev;

/* 
 * Dedicated buffer pool for high-speed IQ data.
 * 4 buffers of 2048 bytes each to allow for double-buffering 
 * + transport overhead.
 */
NET_BUF_POOL_FIXED_DEFINE(iq_buf_pool, 4, 2048, 8, nullptr);

static void fixCodeTriple(struct usbd_context* udsCtx, const enum usbd_speed speed) {
  if (IS_ENABLED(CONFIG_USBD_CDC_ACM_CLASS)) {
    usbd_device_set_code_triple(udsCtx, speed, USB_BCC_MISCELLANEOUS, 0x02, 0x01);
  } else {
    usbd_device_set_code_triple(udsCtx, speed, 0, 0, 0);
  }
}

void USBManager::init() {
  int err = usbd_add_descriptor(usbCtx, &nexrxLang);
  if (err != 0) {
    LOG_ERR("usbd_add_descriptor(lang) failed: %d", err);
    return;
  }

  err = usbd_add_descriptor(usbCtx, &nexrxMfg);
  if (err != 0) {
    LOG_ERR("usbd_add_descriptor(mfg) failed: %d", err);
    return;
  }

  err = usbd_add_descriptor(usbCtx, &nexrxProduct);
  if (err != 0) {
    LOG_ERR("usbd_add_descriptor(product) failed: %d", err);
    return;
  }

  if (USBD_SUPPORTS_HIGH_SPEED && (usbd_caps_speed(usbCtx) == USBD_SPEED_HS)) {
    err = usbd_add_configuration(usbCtx, USBD_SPEED_HS, &nexrxHsConfig);
    if (err != 0) {
      LOG_ERR("usbd_add_configuration(HS) failed: %d", err);
      return;
    }

    err = usbd_register_all_classes(usbCtx, USBD_SPEED_HS, 1, nullptr);
    if (err != 0) {
      LOG_ERR("usbd_register_all_classes(HS) failed: %d", err);
      return;
    }

    fixCodeTriple(usbCtx, USBD_SPEED_HS);
  }

  err = usbd_add_configuration(usbCtx, USBD_SPEED_FS, &nexrxFsConfig);
  if (err != 0) {
    LOG_ERR("usbd_add_configuration(FS) failed: %d", err);
    return;
  }

  err = usbd_register_all_classes(usbCtx, USBD_SPEED_FS, 1, nullptr);
  if (err != 0) {
    LOG_ERR("usbd_register_all_classes(FS) failed: %d", err);
    return;
  }

  fixCodeTriple(usbCtx, USBD_SPEED_FS);

  err = usbd_init(usbCtx);
  if (err != 0) {
    LOG_ERR("usbd_init failed: %d", err);
    return;
  }

  err = usbd_enable(usbCtx);
  if (err != 0) {
    LOG_ERR("USB: Failed to enable stack (%d)", err);
    return;
  }

  LOG_INF("USB: Device Next Initialized and Enabled (High-Speed Composite)");
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
  net_buf_unref(buf);

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
  OSDCapture::usbBusy = false;
}

} // namespace nexrx
