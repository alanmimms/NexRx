#include "UsbCdcTransport.hpp"
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(nexrx_main, LOG_LEVEL_INF);

namespace nexrx {

UsbCdcTransport::UsbCdcTransport() : dev(NULL) {}

bool UsbCdcTransport::init() {
  dev = DEVICE_DT_GET(DT_NODELABEL(cdc_acm_uart1));
  if (!device_is_ready(dev)) {
    LOG_ERR("Transport: CDC_ACM_1 not ready");
    return false;
  }
  LOG_INF("Transport: USB Control Plane initialized (CDC_ACM_1)");
  return true;
}

int UsbCdcTransport::send(const uint8_t* data, size_t len) {
  if (!dev) return -1;
  
  size_t sent = 0;
  while (sent < len) {
    int ret = uart_fifo_fill(dev, data + sent, len - sent);
    if (ret < 0) break;
    sent += ret;
  }
  return static_cast<int>(sent);
}

int UsbCdcTransport::receive(uint8_t* data, size_t maxLen) {
  if (!dev) return -1;
  return uart_fifo_read(dev, data, maxLen);
}

} // namespace nexrx
