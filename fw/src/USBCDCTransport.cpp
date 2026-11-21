#include "USBCDCTransport.hpp"
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(nexrx_main, LOG_LEVEL_INF);

namespace nexrx {

USBCDCTransport::USBCDCTransport()
  : dev(nullptr) {
}

bool USBCDCTransport::init() {
  dev = DEVICE_DT_GET(DT_NODELABEL(control_plane_uart));
  if (!device_is_ready(dev)) {
    LOG_ERR("Transport: control_plane_uart not ready");
    return false;
  }
  uart_irq_rx_enable(dev);
  LOG_INF("Transport: USB Control Plane initialized (control_plane_uart)");
  return true;
}

int USBCDCTransport::send(const uint8_t* data, size_t len) {
  if (!dev) {
    return -1;
  }
  
  size_t sent = 0;
  while (sent < len) {
    int ret = uart_fifo_fill(dev, data + sent, len - sent);
    if (ret < 0) {
      break;
    }
    sent += ret;
  }
  uart_irq_tx_enable(dev);
  return static_cast<int>(sent);
}

int USBCDCTransport::receive(uint8_t* data, size_t maxLen) {
  if (!dev) {
    return -1;
  }
  int ret = uart_fifo_read(dev, data, maxLen);
  if (ret > 0) {
    uart_irq_rx_enable(dev);
  }
  return ret;
}

} // namespace nexrx
