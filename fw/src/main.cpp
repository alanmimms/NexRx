#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "Si5351Driver.hpp"
#include "CPLDDriver.hpp"
#include "TLV320ADC5140.hpp"
#include "DisplayManager.hpp"
#include "USBManager.hpp"
#include "OSDCapture.hpp"
#include "ControlHandler.hpp"
#include "USBCDCTransport.hpp"

LOG_MODULE_REGISTER(nexrx_main, LOG_LEVEL_INF);

int main() {
  /* 1. Initialize USB Connectivity First for Logging */
  nexrx::USBManager::init();
  LOG_INF("NexRx MCU Firmware Starting (STM32H743VIT6)...");

  /* 2. Initialize Display Early for Status Feedback */
  nexrx::DisplayManager::init();
  nexrx::DisplayManager::showStatus("BOOTING...");

  /* 3. Initialize Synthesizer and Dual CPLDs */
  nexrx::Si5351Driver::init();
  nexrx::CPLDDriver::init();

  /* 4. Configure Dual TLV320ADC5140 Audio ADCs with Integrated PGAs */
  nexrx::TLV320ADC5140::init();

  /* 5. Initialize OSD Audio DMA Data Capture */
  nexrx::OSDCapture::init();

  /* 6. Initialize Control Plane Transport */
  static nexrx::USBCDCTransport controlTransport;
  controlTransport.init();

  LOG_INF("System Initialization Complete. Automatic power hardware ready.");
  nexrx::DisplayManager::showStatus("READY");

  while (true) {
    /* Process incoming control messages */
    nexrx::ControlHandler::instance().process(controlTransport);
    k_sleep(K_MSEC(10));
  }

  return 0;
}
