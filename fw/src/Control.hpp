#pragma once

#include <stdint.h>

namespace nexrx {

/* Helper to convert 4-character strings to uint32_t at compile time */
static constexpr uint32_t makeControlId(const char s[5]) {
  return (static_cast<uint32_t>(s[0]) << 24) |
         (static_cast<uint32_t>(s[1]) << 16) |
         (static_cast<uint32_t>(s[2]) << 8)  |
         (static_cast<uint32_t>(s[3]));
}

class Control {
public:
  /* Command IDs */
  static constexpr uint32_t CMD_SET_VFO         = makeControlId("SVFO");
  static constexpr uint32_t CMD_SET_ATTEN       = makeControlId("SATT");
  static constexpr uint32_t CMD_SET_PGA_GAIN    = makeControlId("SPGA");
  static constexpr uint32_t CMD_SET_AGC_MODE    = makeControlId("SAGC");
  static constexpr uint32_t CMD_SET_TR_MODE     = makeControlId("STRM");
  static constexpr uint32_t CMD_START_STREAM    = makeControlId("STM[");
  static constexpr uint32_t CMD_STOP_STREAM     = makeControlId("]STM");
  static constexpr uint32_t CMD_GET_TIMESTAMP   = makeControlId("GTIM");
  static constexpr uint32_t CMD_SET_ISG_FREQ    = makeControlId("SIFQ");
  static constexpr uint32_t CMD_SET_ISG_ENABLE  = makeControlId("SIEN");
  static constexpr uint32_t CMD_SET_HPF_BYPASS  = makeControlId("SHPB");
  static constexpr uint32_t CMD_SET_BPF_SELECT  = makeControlId("SBPF");
  static constexpr uint32_t CMD_GET_STATE       = makeControlId("GSTA");
  static constexpr uint32_t CMD_CAL_STIM        = makeControlId("CAL!");
  static constexpr uint32_t CMD_GBYE            = makeControlId("GBYE");
};

} // namespace nexrx
