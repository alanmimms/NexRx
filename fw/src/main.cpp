#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "app/PowerManager.hpp"
#include "drivers/FPGAManager.hpp"
#include "drivers/AK5578.hpp"
#include "drivers/MAX9939.hpp"
#include "drivers/NexBus.hpp"
#include "transport/USBManager.hpp"
#include "transport/QSDCapture.hpp"

LOG_MODULE_REGISTER(nexrx_main, LOG_LEVEL_INF);

int main() {
  /* 1. Initialize USB Connectivity First for Logging */
  nexrx::USBManager::init();
  LOG_INF("NexRx MCU Firmware Starting (Modular Refactor)...");

  /* 2. Hardware Power-up */
  nexrx::PowerManager::init();
  nexrx::PowerManager::monitorUSBPower();
  nexrx::PowerManager::runSequence();

  /* 3. Load FPGA and Validate Path */
  nexrx::FPGAManager::init();
  /* TODO: Pass real bitstream array from flash */
  nexrx::FPGAManager::loadBitstream(nullptr, 0);

  /* 4. Configure Analog Front End */
  nexrx::AK5578::init();
  nexrx::MAX9939::init();

  /* 5. Initialize Distributed Control & Data Capture */
  nexrx::NexBus::init();
  nexrx::QSDCapture::init();

  LOG_INF("System Initialization Complete.");

  while (true) {
    k_sleep(K_MSEC(1000));
  }

  return 0;
}
