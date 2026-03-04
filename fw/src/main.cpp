#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "app/PowerManager.hpp"
#include "drivers/FPGAManager.hpp"
#include "drivers/AK5578.hpp"
#include "drivers/MAX9939.hpp"
#include "drivers/NexBus.hpp"
#include "drivers/DisplayManager.hpp"
#include "transport/USBManager.hpp"
#include "transport/QSDCapture.hpp"
#include "transport/ControlHandler.hpp"
#include "transport/UsbCdcTransport.hpp"

LOG_MODULE_REGISTER(nexrx_main, LOG_LEVEL_INF);

int main() {
  /* 1. Initialize USB Connectivity First for Logging */
  nexrx::USBManager::init();
  LOG_INF("NexRx MCU Firmware Starting...");

  /* 2. Hardware Power-up */
  nexrx::PowerManager::init();
  nexrx::PowerManager::monitorUSBPower();
  nexrx::PowerManager::runSequence();

  /* 3. Initialize Display Early for Status Feedback */
  nexrx::DisplayManager::init();
  nexrx::DisplayManager::showStatus("BOOTING...");

  /* 4. Load FPGA and Validate Path */
  nexrx::FPGAManager::init();
  nexrx::FPGAManager::loadBitstream(nullptr, 0);

  /* 5. Configure Analog Front End */
  nexrx::AK5578::init();
  nexrx::MAX9939::init();

  /* 6. Initialize Distributed Control & Data Capture */
  nexrx::NexBus::init();
  nexrx::QSDCapture::init();

  /* 7. Initialize Control Plane Transport */
  static nexrx::UsbCdcTransport controlTransport;
  controlTransport.init();

  LOG_INF("System Initialization Complete.");
  nexrx::DisplayManager::showStatus("READY");

  while (true) {
    /* Process incoming control messages */
    nexrx::ControlHandler::instance().process(controlTransport);
    k_sleep(K_MSEC(10));
  }

  return 0;
}
