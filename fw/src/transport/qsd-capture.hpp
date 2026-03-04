#pragma once

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2s.h>
#include "IQPacketHeader.hpp"

namespace nexrx {

class QSDCapture {
public:
  static constexpr size_t SAMPLES_PER_HALF = 256;
  static constexpr size_t CHANNEL_COUNT = 6;
  
  /* SAI Capture Buffers (Double-Buffered) */
  static uint32_t laneBuffers[4][SAMPLES_PER_HALF * 2];

  /* USB Offload Buffers (Interleaved Output) */
  static constexpr size_t PACKET_SIZE = sizeof(IQPacketHeader) + 
                                        (CHANNEL_COUNT * SAMPLES_PER_HALF * 4);
  static uint8_t usbBufferA[PACKET_SIZE];
  static uint8_t usbBufferB[PACKET_SIZE];

  static uint32_t currentSequence;
  static uint32_t totalOverruns;
  static bool usbBusy;

  static struct k_sem dataReadySem;
  static uint8_t activeHalf;

  static void init();
  static void start();
  static void pumpThread(void*, void*, void*);
  static void processHalf(uint8_t halfIndex);
  static void submitToUSB(uint8_t* data, size_t len);

private:
  static const struct device* saiDevs[4];
  static struct i2s_config saiConfig;
};

} // namespace nexrx
