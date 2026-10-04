#include "OSDCapture.hpp"
#include <zephyr/logging/log.h>
#include <cmath>
#include <algorithm>
#include "USBManager.hpp"
#include "AGCManager.hpp"

LOG_MODULE_REGISTER(osd_capture, LOG_LEVEL_INF);

namespace nexrx {

namespace {

// Angular step for 12 kHz frequency rotation at 384 ksps:
// Delta = 2 * pi * (12000 / 384000) = 2 * pi / 32 = pi / 16
constexpr double piVal = 3.14159265358979323846;
constexpr double rotStepRad = (2.0 * piVal * 12000.0) / 384000.0;
const double deltaCos = std::cos(rotStepRad);
const double deltaSin = std::sin(rotStepRad);

constexpr double invSqrt2 = 0.70710678118654752440; // 1 / sqrt(2)
constexpr double fourPhaseGainNorm = 1.41421356237309504880; // +3 dB

} // namespace

uint32_t OSDCapture::osd0Lanes[4][OSDCapture::samplesPerHalf * 2];
uint32_t OSDCapture::osd1Lanes[4][OSDCapture::samplesPerHalf * 2];

uint8_t OSDCapture::usbBufferA[OSDCapture::packetSize];
uint8_t OSDCapture::usbBufferB[OSDCapture::packetSize];

uint32_t OSDCapture::currentSequence = 0;
uint32_t OSDCapture::totalOverruns   = 0;
bool OSDCapture::streamingActive     = false;
bool OSDCapture::fourPhaseMode       = false;
bool OSDCapture::usbBusy             = false;

struct k_sem OSDCapture::dataReadySem;
uint8_t OSDCapture::activeHalf = 0;

double OSDCapture::rotCos = 1.0;
double OSDCapture::rotSin = 0.0;

K_THREAD_STACK_DEFINE(osdStack, 4096);
static struct k_thread osdThreadData;

void OSDCapture::init() {
  LOG_INF("Initializing Dual OSD Audio Capture & Cortex-M7 DSP Engine...");
  k_sem_init(&dataReadySem, 0, 1);

  currentSequence = 0;
  totalOverruns = 0;
  streamingActive = false;
  fourPhaseMode = false;
  rotCos = 1.0;
  rotSin = 0.0;

  k_thread_create(&osdThreadData, osdStack, K_THREAD_STACK_SIZEOF(osdStack),
                  OSDCapture::pumpThread, nullptr, nullptr, nullptr,
                  K_PRIO_COOP(2), 0, K_NO_WAIT);

  LOG_INF("OSD Audio Capture & DSP Engine Initialized.");
}

void OSDCapture::start() {
  currentSequence = 0;
  rotCos = 1.0;
  rotSin = 0.0;
  streamingActive = true;
  LOG_INF("OSD Audio Stream STARTED (384 ksps I/Q).");
}

void OSDCapture::stop() {
  streamingActive = false;
  LOG_INF("OSD Audio Stream STOPPED.");
}

void OSDCapture::setFourPhaseMode(bool fourPhase) {
  fourPhaseMode = fourPhase;
  LOG_INF("OSD DSP: Mode set to %s (Gain normalization %s)",
          fourPhase ? "4-Phase QSD" : "8-Phase OSD",
          fourPhase ? "+3dB active" : "none");
}

void OSDCapture::onDMABufferComplete(uint8_t halfIndex) {
  if (!streamingActive) {
    return;
  }
  activeHalf = halfIndex;
  k_sem_give(&dataReadySem);
}

void OSDCapture::pumpThread(void*, void*, void*) {
  while (true) {
    if (k_sem_take(&dataReadySem, K_FOREVER) == 0) {
      if (streamingActive) {
        processHalf(activeHalf);
      }
    }
  }
}

void OSDCapture::processHalf(uint8_t halfIndex) {
  uint8_t* targetBuf = (currentSequence % 2 == 0) ? usbBufferA : usbBufferB;
  IQPacketHeader* header = reinterpret_cast<IQPacketHeader*>(targetBuf);
  header->magic        = IQPacketHeader::MAGIC;
  header->version      = 2;
  header->sequence     = currentSequence++;
  header->timestampNS  = k_ticks_to_ns_floor64(k_uptime_ticks());
  header->frameCount   = samplesPerHalf;
  header->overrunCount = totalOverruns;

  int32_t* samplePayload = reinterpret_cast<int32_t*>(targetBuf + sizeof(IQPacketHeader));
  size_t baseOffset = halfIndex * samplesPerHalf;
  int32_t maxPeak = 0;

  double locCos = rotCos;
  double locSin = rotSin;

  for (int i = 0; i < static_cast<int>(samplesPerHalf); ++i) {
    size_t idx = baseOffset + i;

    // Read 4 channels for OSD0
    double c0_0 = static_cast<double>(static_cast<int32_t>(osd0Lanes[0][idx]));
    double c0_1 = static_cast<double>(static_cast<int32_t>(osd0Lanes[1][idx]));
    double c0_2 = static_cast<double>(static_cast<int32_t>(osd0Lanes[2][idx]));
    double c0_3 = static_cast<double>(static_cast<int32_t>(osd0Lanes[3][idx]));

    // Read 4 channels for OSD1
    double c1_0 = static_cast<double>(static_cast<int32_t>(osd1Lanes[0][idx]));
    double c1_1 = static_cast<double>(static_cast<int32_t>(osd1Lanes[1][idx]));
    double c1_2 = static_cast<double>(static_cast<int32_t>(osd1Lanes[2][idx]));
    double c1_3 = static_cast<double>(static_cast<int32_t>(osd1Lanes[3][idx]));

    double i0 = 0.0, q0 = 0.0, i1 = 0.0, q1 = 0.0;

    // Polyphase projection into orthogonal I/Q
    if (fourPhaseMode) {
      i0 = c0_0;
      q0 = c0_2;
      i1 = c1_0;
      q1 = c1_2;
    } else {
      i0 = c0_0 + invSqrt2 * (c0_1 - c0_3);
      q0 = c0_2 + invSqrt2 * (c0_1 + c0_3);
      i1 = c1_0 + invSqrt2 * (c1_1 - c1_3);
      q1 = c1_2 + invSqrt2 * (c1_1 + c1_3);
    }

    // Shift OSD0 DOWN by 12 kHz: (I + jQ) * (cos - j*sin)
    double s0_i = i0 * locCos + q0 * locSin;
    double s0_q = q0 * locCos - i0 * locSin;

    // Shift OSD1 UP by 12 kHz: (I + jQ) * (cos + j*sin)
    double s1_i = i1 * locCos - q1 * locSin;
    double s1_q = q1 * locCos + i1 * locSin;

    // Advance complex phasor rotation
    double nextCos = locCos * deltaCos - locSin * deltaSin;
    double nextSin = locSin * deltaCos + locCos * deltaSin;
    locCos = nextCos;
    locSin = nextSin;

    // Renormalize rotation phasor periodically to eliminate drift
    if ((i & 0x7F) == 0) {
      double mag = 1.0 / std::sqrt(locCos * locCos + locSin * locSin);
      locCos *= mag;
      locSin *= mag;
    }

    // Coherently sum both OSD streams
    double combI = (s0_i + s1_i) * 0.5;
    double combQ = (s0_q + s1_q) * 0.5;

    // Apply 4-phase mode gain normalization (+3 dB)
    if (fourPhaseMode) {
      combI *= fourPhaseGainNorm;
      combQ *= fourPhaseGainNorm;
    }

    // Clamp to 24-bit audio range (-8388608 to 8388607) packed into int32_t
    int32_t finalI = static_cast<int32_t>(std::clamp(std::round(combI), -8388608.0, 8388607.0));
    int32_t finalQ = static_cast<int32_t>(std::clamp(std::round(combQ), -8388608.0, 8388607.0));

    int32_t peak = std::max(std::abs(finalI), std::abs(finalQ));
    if (peak > maxPeak) {
      maxPeak = peak;
    }

    samplePayload[i * 2 + 0] = finalI;
    samplePayload[i * 2 + 1] = finalQ;
  }

  rotCos = locCos;
  rotSin = locSin;

  // Reflex AGC fast loop peak detection
  AGCManager::processReflex(maxPeak);

  // Submit to USB High-Speed Bulk IN pipe
  USBManager::submitBulkIn(targetBuf, packetSize);
}

uint32_t OSDCapture::getOverrunCount() {
  return totalOverruns;
}

uint32_t OSDCapture::getSequence() {
  return currentSequence;
}

} // namespace nexrx
