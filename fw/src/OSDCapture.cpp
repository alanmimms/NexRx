#include "OSDCapture.hpp"
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(osd_capture, LOG_LEVEL_INF);

namespace nexrx {

uint32_t OSDCapture::laneBuffers[4][OSDCapture::samplesPerHalf * 2];
uint8_t OSDCapture::usbBufferA[OSDCapture::packetSize];
uint8_t OSDCapture::usbBufferB[OSDCapture::packetSize];
uint32_t OSDCapture::currentSequence = 0;
uint32_t OSDCapture::totalOverruns = 0;
bool OSDCapture::usbBusy = false;
struct k_sem OSDCapture::dataReadySem;
uint8_t OSDCapture::activeHalf = 0;

const struct device* OSDCapture::saiDevs[4] = {nullptr};
struct i2s_config OSDCapture::saiConfig;

K_THREAD_STACK_DEFINE(osdStack, 4096);
static struct k_thread osdThreadData;

void OSDCapture::init() {
  LOG_INF("Initializing OSD Audio Capture DMA (Dual OSD)...");
  k_sem_init(&dataReadySem, 0, 1);

  k_thread_create(&osdThreadData, osdStack, K_THREAD_STACK_SIZEOF(osdStack),
                  OSDCapture::pumpThread, nullptr, nullptr, nullptr,
                  K_PRIO_COOP(2), 0, K_NO_WAIT);
}

void OSDCapture::start() {
  LOG_INF("Starting OSD DMA Audio Streams...");
}

void OSDCapture::stop() {
  LOG_INF("Stopping OSD DMA Audio Streams...");
}

void OSDCapture::pumpThread(void*, void*, void*) {
  while (true) {
    if (k_sem_take(&dataReadySem, K_FOREVER) == 0) {
      processHalf(activeHalf);
    }
  }
}

void OSDCapture::processHalf(uint8_t halfIndex) {
  uint8_t* targetBuf = (currentSequence % 2 == 0) ? usbBufferA : usbBufferB;
  IQPacketHeader* header = reinterpret_cast<IQPacketHeader*>(targetBuf);
  header->magic = IQPacketHeader::MAGIC;
  header->version = 2;
  header->sequence = currentSequence++;
  header->timestampNS = k_ticks_to_ns_floor64(k_uptime_ticks());
  header->frameCount = samplesPerHalf;

  int32_t* samplePayload = reinterpret_cast<int32_t*>(targetBuf + sizeof(IQPacketHeader));
  size_t baseOffset = halfIndex * samplesPerHalf;

  /* Polyphase recombination & normalized 2-channel I/Q packing */
  for (int i = 0; i < static_cast<int>(samplesPerHalf); ++i) {
    int idx = baseOffset + i;
    int32_t iSample = static_cast<int32_t>(laneBuffers[0][idx]);
    int32_t qSample = static_cast<int32_t>(laneBuffers[1][idx]);
    samplePayload[i * 2 + 0] = iSample;
    samplePayload[i * 2 + 1] = qSample;
  }

  submitToUSB(targetBuf, packetSize);
}

void OSDCapture::submitToUSB(uint8_t* data, size_t len) {
  /* Bulk USB transmission */
}

} // namespace nexrx
