#pragma once
#include <cstdint>
#include <cstddef>

// You can treat this class as a "functor" to compute CRC-16-CCITT
// checksum on a buffer and immediately return its checksum:
//
//    uint16_t ckSum = CommandChecksum(&buf, length);
//
// Or you can construct an "empty" CommandChecksum and accumulate
// chunks of data in the case of streaming or non-contiguous buffers,
// then catch the result in a uint16_t and Bob's yer uncle.
//
//    CommandChecksum sumBox{};
//    ... sumBox.accumulateData(&buf, length); ...
//    uint16_t ckSum = sumBox;

class CommandChecksum {
  uint16_t sum1 = 0;
  uint16_t sum2 = 0;

 public:
  // Parameterless constructor for step-by-step accumulation ("the hard way").
  constexpr CommandChecksum() : sum1(0), sum2(0) {}

  // Parameterized constructor for immediate calculation from data and length.
  constexpr CommandChecksum(const uint8_t *dataP, size_t length) : sum1(0), sum2(0) {
    accumulateData(dataP, length);
  }

  // Stateful accumulation method for streaming or non-contiguous buffers.
  constexpr void accumulateData(const uint8_t* dataP, size_t length) {
    for (size_t byteIndex = 0; byteIndex < length; ++byteIndex) {
      sum1 += dataP[byteIndex];
      sum2 += sum1;
    }
  }
    
  // Extract the final result when ready.
  constexpr operator uint16_t() const {
    return ((sum2 % 255) << 8) | (sum1 % 255);
  }
};
