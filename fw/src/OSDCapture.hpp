#pragma once

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2s.h>
#include "IQPacketHeader.hpp"

namespace nexrx {

class OSDCapture {
public:
  static constexpr size_t samplesPerHalf = 384;
  static constexpr size_t channelCount = 2; /* 1 * (I+Q) normalized stream */
  static constexpr size_t packetSize = sizeof(IQPacketHeader) + (samplesPerHalf * channelCount * 4);

  static void init();
  static void start();
  static void stop();

  static uint32_t laneBuffers[4][samplesPerHalf * 2];
  static uint8_t usbBufferA[packetSize];
  static uint8_t usbBufferB[packetSize];
  static uint32_t currentSequence;
  static uint32_t totalOverruns;
  static bool usbBusy;
  static struct k_sem dataReadySem;
  static uint8_t activeHalf;

  static void pumpThread(void*, void*, void*);
  static void processHalf(uint8_t halfIndex);
  static void submitToUSB(uint8_t* data, size_t len);

private:
  static const struct device* saiDevs[4];
  static struct i2s_config saiConfig;
};

} // namespace nexrx
