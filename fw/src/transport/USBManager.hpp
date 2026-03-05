#pragma once

#include <zephyr/kernel.h>
#include <zephyr/usb/usbd.h>

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

private:
  static struct usbd_context* usbCtx;
  static constexpr uint8_t bulkInEp = 0x81; /* Assigned in HS mode */
};

} // namespace nexrx
