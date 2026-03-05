#pragma once

#include <stdint.h>

namespace nexrx {

struct IQPacketHeader {
  static constexpr uint32_t MAGIC = 0x52584E51; /* "NXRQ" */
  uint32_t magic;         
  uint32_t version;       /* 2 (Binary Data Plane) */
  uint32_t sequence;      /* Packet sequence number */
  uint64_t timestampNS;   /* Nanoseconds since boot */
  uint32_t frameCount;    /* Number of frames in payload */
  uint32_t overrunCount;  /* Count of MCU-side buffer overruns */
} __attribute__((packed));

} // namespace nexrx
