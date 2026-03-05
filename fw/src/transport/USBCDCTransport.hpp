#pragma once

#include "ControlHandler.hpp"
#include <zephyr/drivers/uart.h>

namespace nexrx {

class USBCDCTransport : public ITransport {
public:
  USBCDCTransport();
  
  bool init();
  
  int send(const uint8_t* data, size_t len) override;
  int receive(uint8_t* data, size_t maxLen) override;

private:
  const struct device* dev;
};

} // namespace nexrx
