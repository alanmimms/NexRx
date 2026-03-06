#pragma once

#include <stdint.h>

namespace nexrx {

class Control {
public:
  /* Command IDs as uint32_t (4-byte ASCII) */
  static constexpr uint32_t CMD_SET_VFO         = 0x5356464F; /* "SVFO" */
  static constexpr uint32_t CMD_SET_ATTEN       = 0x53415454; /* "SATT" */
  static constexpr uint32_t CMD_SET_PGA_GAIN    = 0x53504741; /* "SPGA" */
  static constexpr uint32_t CMD_SET_AGC_MODE    = 0x53414743; /* "SAGC" */
  static constexpr uint32_t CMD_SET_TR_MODE     = 0x5354524D; /* "STRM" */
  static constexpr uint32_t CMD_START_STREAM    = 0x53544D5B; /* "STM[" */
  static constexpr uint32_t CMD_STOP_STREAM     = 0x5D53544D; /* "]STM" */
  static constexpr uint32_t CMD_GET_TIMESTAMP   = 0x4754494D; /* "GTIM" */
  static constexpr uint32_t CMD_SET_ISG_FREQ    = 0x53494651; /* "SIFQ" */
  static constexpr uint32_t CMD_SET_ISG_ENABLE  = 0x5349454E; /* "SIEN" */
  static constexpr uint32_t CMD_SET_PRESEL_L    = 0x5350524C; /* "SPRL" */
  static constexpr uint32_t CMD_SET_PRESEL_C    = 0x53505243; /* "SPRC" */
  static constexpr uint32_t CMD_SET_PRESEL_EN   = 0x53505245; /* "SPRE" */
  static constexpr uint32_t CMD_GET_STATE       = 0x47535441; /* "GSTA" */
  static constexpr uint32_t CMD_GBYE            = 0x47425945; /* "GBYE" */
};

} // namespace nexrx
