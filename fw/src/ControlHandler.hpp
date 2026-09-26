#pragma once

#include <zephyr/kernel.h>
#include <stdint.h>

namespace nexrx {

class ITransport {
public:
  virtual ~ITransport() = default;
  virtual int send(const uint8_t* data, size_t len) = 0;
  virtual int receive(uint8_t* data, size_t maxLen) = 0;
};

class ControlHandler {
public:
  static ControlHandler& instance();
  void process(ITransport& transport);

private:
  ControlHandler() = default;
};

} // namespace nexrx
