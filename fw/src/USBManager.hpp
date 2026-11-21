#pragma once

#include <zephyr/kernel.h>
#include <zephyr/usb/usbd.h>
#include <stdint.h>

namespace nexrx {

class USBManager {
public:
  static void init();
  
  /**
   * @brief Submits a buffer to the high-speed Bulk IN endpoint.
   * @param data Pointer to the interleaved IQ packet.
   * @param len Length of the packet.
   * @return 0 on success, negative error code otherwise.
   */
  static int submitBulkIn(uint8_t* data, size_t len);

  /**
   * @brief Callback from USB stack when a transfer is complete.
   */
  static void onBulkInComplete(struct usbd_context* udsCtx, 
                               struct net_buf* buf, int err);

  // NexRx vendor/device from pidcodes.github.com.
  static constexpr uint16_t vendor = 0x1209;
  static constexpr uint16_t device = 0x4E75;

private:
  static struct usbd_context* usbCtx;
  static constexpr uint8_t bulkInEp = 0x81; /* Assigned in HS mode */
};

} // namespace nexrx
