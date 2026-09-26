#ifndef NEXRX_AK5578_HPP
#define NEXRX_AK5578_HPP

#include <zephyr/kernel.h>
#include <stdint.h>

namespace nexrx {

class AK5578 {
public:
  static void init();
  static void reset();
};

} // namespace nexrx

#endif // NEXRX_AK5578_HPP
