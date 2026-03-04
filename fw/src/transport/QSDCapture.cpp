#include "QSDCapture.hpp"
#include "USBManager.hpp"
#include <zephyr/logging/log.h>
#include <zephyr/sys/time_units.h>
#include <zephyr/drivers/i2s.h>

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

  /* Map SAI devices */
  saiDevs[0] = DEVICE_DT_GET(DT_NODELABEL(sai3_a)); 
  saiDevs[1] = DEVICE_DT_GET(DT_NODELABEL(sai2_b)); 
  saiDevs[2] = DEVICE_DT_GET(DT_NODELABEL(sai2_a)); 
  saiDevs[3] = DEVICE_DT_GET(DT_NODELABEL(sai4_a)); 

  for (int i = 0; i < 4; i++) {
    if (!device_is_ready(saiDevs[i])) {
      LOG_ERR("QSD Capture: SAI Device %d not ready", i);
      return;
    }
  }

  /* Configuration Logic */
  LOG_INF("QSD Capture: Engine Initialized");
}

void QSDCapture::start() {
  LOG_INF("QSD Capture: Starting synchronous streams...");
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
  header->timestampNS = k_ticks_to_ns_near64(k_uptime_ticks());
  header->frameCount = SAMPLES_PER_HALF;
  header->overrunCount = totalOverruns;

  size_t laneOffset = halfIndex * SAMPLES_PER_HALF * 2;

  /* Interleave lanes into 6 channels */
  for (size_t i = 0; i < SAMPLES_PER_HALF; ++i) {
    size_t d = i * CHANNEL_COUNT;
    size_t s = laneOffset + (i * 2);
    samples[d + 0] = static_cast<int32_t>(laneBuffers[0][s + 0]);
    samples[d + 1] = static_cast<int32_t>(laneBuffers[0][s + 1]);
    samples[d + 2] = static_cast<int32_t>(laneBuffers[1][s + 0]);
    samples[d + 3] = static_cast<int32_t>(laneBuffers[1][s + 1]);
    samples[d + 4] = static_cast<int32_t>(laneBuffers[2][s + 0]);
    samples[d + 5] = static_cast<int32_t>(laneBuffers[2][s + 1]);
  }

  /* Submit to high-speed USB pipeline */
  usbBusy = true;
  if (USBManager::submitBulkIn(activeUsbBuf, PACKET_SIZE) != 0) {
    usbBusy = false;
  }
}

void QSDCapture::submitToUSB(uint8_t* data, size_t len) {
  /* Deprecated: Integrated into processHalf */
}

K_THREAD_DEFINE(qsd_pump_tid, 2048, QSDCapture::pumpThread, NULL, NULL, NULL,
                -1, K_FP_REGS, 0);

} // namespace nexrx
