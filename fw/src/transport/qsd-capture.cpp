#include "qsd-capture.hpp"
#include "usb-manager.hpp"
#include "../drivers/fpga-manager.hpp"
#include "../app/agc-manager.hpp"
#include <zephyr/logging/log.h>
#include <zephyr/sys/time_units.h>
#include <zephyr/drivers/i2s.h>
#include <algorithm>

LOG_MODULE_DECLARE(nexrx_main, LOG_LEVEL_INF);

namespace nexrx {

/* Define static members */
uint32_t QSDCapture::laneBuffers[4][SAMPLES_PER_HALF * 2];
uint8_t QSDCapture::usbBufferA[QSDCapture::PACKET_SIZE];
uint8_t QSDCapture::usbBufferB[QSDCapture::PACKET_SIZE];
uint32_t QSDCapture::currentSequence = 0;
uint32_t QSDCapture::totalOverruns = 0;
bool QSDCapture::usbBusy = false;
struct k_sem QSDCapture::dataReadySem;
uint8_t QSDCapture::activeHalf = 0;

const struct device* QSDCapture::saiDevs[4];
struct i2s_config QSDCapture::saiConfig;

void QSDCapture::init() {
  k_sem_init(&dataReadySem, 0, 1);
  currentSequence = 0;
  totalOverruns = 0;
  usbBusy = false;

  saiDevs[0] = DEVICE_DT_GET(DT_NODELABEL(sai3_a)); 
  saiDevs[1] = DEVICE_DT_GET(DT_NODELABEL(sai2_b)); 
  saiDevs[2] = DEVICE_DT_GET(DT_NODELABEL(sai2_a)); 
  saiDevs[3] = DEVICE_DT_GET(DT_NODELABEL(sai4_a)); 

  for (int i = 0; i < 4; i++) {
    if (!device_is_ready(saiDevs[i])) return;
  }
}

void QSDCapture::start() {
  i2s_trigger(saiDevs[1], I2S_DIR_RX, I2S_TRIGGER_START);
  i2s_trigger(saiDevs[2], I2S_DIR_RX, I2S_TRIGGER_START);
  i2s_trigger(saiDevs[3], I2S_DIR_RX, I2S_TRIGGER_START);
  i2s_trigger(saiDevs[0], I2S_DIR_RX, I2S_TRIGGER_START);
}

void QSDCapture::pumpThread(void*, void*, void*) {
  while (true) {
    k_sem_take(&dataReadySem, K_FOREVER);
    processHalf(activeHalf);
  }
}

void QSDCapture::processHalf(uint8_t halfIndex) {
  if (usbBusy) {
    totalOverruns++;
    return;
  }

  uint8_t* activeUsbBuf = (currentSequence % 2 == 0) ? usbBufferA : usbBufferB;
  IQPacketHeader* header = reinterpret_cast<IQPacketHeader*>(activeUsbBuf);
  int32_t* samples = reinterpret_cast<int32_t*>(activeUsbBuf + 
                                                sizeof(IQPacketHeader));

  header->magic = IQPacketHeader::MAGIC;
  header->version = 2;
  header->sequence = currentSequence++;
  header->timestampNS = FPGAManager::getTCXOTimestamp();
  header->frameCount = SAMPLES_PER_HALF;
  header->overrunCount = totalOverruns;

  size_t laneOffset = halfIndex * SAMPLES_PER_HALF * 2;
  int32_t peak = 0;

  for (size_t i = 0; i < SAMPLES_PER_HALF; ++i) {
    size_t d = i * CHANNEL_COUNT;
    size_t s = laneOffset + (i * 2);
    
    for (int ch = 0; ch < 3; ch++) {
      int32_t iVal = static_cast<int32_t>(laneBuffers[ch][s + 0]);
      int32_t qVal = static_cast<int32_t>(laneBuffers[ch][s + 1]);
      samples[d + (ch*2) + 0] = iVal;
      samples[d + (ch*2) + 1] = qVal;
      peak = std::max({peak, std::abs(iVal), std::abs(qVal)});
    }
  }

  AGCManager::processReflex(peak);

  usbBusy = true;
  if (USBManager::submitBulkIn(activeUsbBuf, PACKET_SIZE) != 0) {
    usbBusy = false;
  }
}

void QSDCapture::submitToUSB(uint8_t* data, size_t len) {}

K_THREAD_DEFINE(qsd_pump_tid, 2048, QSDCapture::pumpThread, NULL, NULL, NULL,
                -1, K_FP_REGS, 0);

} // namespace nexrx
