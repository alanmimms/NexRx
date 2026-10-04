#pragma once

#include <stdint.h>
#include <zephyr/kernel.h>
#include "IQPacketHeader.hpp"

namespace nexrx {

class OSDCapture {
public:
  static constexpr size_t samplesPerHalf = 384; /* 1.0 ms at 384 ksps */
  static constexpr size_t channelCount   = 2;   /* Interleaved I/Q */
  static constexpr size_t packetSize     = sizeof(IQPacketHeader) + (samplesPerHalf * channelCount * sizeof(int32_t));

  static void init();
  static void start();
  static void stop();
  static void setFourPhaseMode(bool fourPhase);

  static void onDMABufferComplete(uint8_t halfIndex);

  static uint32_t getOverrunCount();
  static uint32_t getSequence();
  static bool usbBusy;

private:
  static void pumpThread(void*, void*, void*);
  static void processHalf(uint8_t halfIndex);

  static uint32_t osd0Lanes[4][samplesPerHalf * 2];
  static uint32_t osd1Lanes[4][samplesPerHalf * 2];

  static uint8_t usbBufferA[packetSize];
  static uint8_t usbBufferB[packetSize];

  static uint32_t currentSequence;
  static uint32_t totalOverruns;
  static bool streamingActive;
  static bool fourPhaseMode;

  static struct k_sem dataReadySem;
  static uint8_t activeHalf;

  // Phasor rotation state for +/- 12 kHz frequency shift
  static double rotCos;
  static double rotSin;
};

} // namespace nexrx
