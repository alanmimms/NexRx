#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>

#include "FrontEndDriver.hpp"
#include "Si5351Driver.hpp"
#include "CPLDDriver.hpp"
#include "TLV320ADC5140.hpp"
#include "DisplayManager.hpp"
#include "USBManager.hpp"
#include "OSDCapture.hpp"
#include "ControlHandler.hpp"
#include "USBCDCTransport.hpp"

LOG_MODULE_REGISTER(nexrx_main, LOG_LEVEL_INF);


static const struct gpio_dt_spec usbPhyReset =
    GPIO_DT_SPEC_GET(DT_NODELABEL(usbhs_ulpi_phy), reset_gpios);

constexpr int resetPulseMs = 5;
constexpr int pllLockDelayMs = 10;

static int resetUsbPhy() {
  if (!gpio_is_ready_dt(&usbPhyReset)) {
    return -ENODEV;
  }

  // Assert /RESET (drive PC4 low because of GPIO_ACTIVE_LOW)
  int err = gpio_pin_configure_dt(&usbPhyReset, GPIO_OUTPUT_ACTIVE);
  if (err != 0) {
    return err;
  }
  k_msleep(resetPulseMs);

  // Deassert /RESET (drive PC4 high) and wait for 60 MHz CLKOUT to stabilize
  err = gpio_pin_set_dt(&usbPhyReset, 0);
  if (err != 0) {
    return err;
  }
  k_msleep(pllLockDelayMs);

  return 0;
}

SYS_INIT(resetUsbPhy, POST_KERNEL, 45);



int main() {
  /* 1. Initialize USB Connectivity First for Logging */
  nexrx::USBManager::init();
  LOG_INF("NexRx MCU Firmware Starting (STM32H743VIT6 / STM32H753VIT6)...");

  /* 2. Initialize Display Early for Status Feedback */
  nexrx::DisplayManager::init();
  nexrx::DisplayManager::showStatus("BOOTING...");

  /* 3. Initialize Front-End Switches (Attenuators, BPF, 3.3V Power) */
  nexrx::FrontEndDriver::init();

  /* 4. Initialize Synthesizer and Dual CPLDs */
  nexrx::Si5351Driver::init();
  nexrx::CPLDDriver::init();

  /* 5. Configure Dual TLV320ADC5140 Audio ADCs with Integrated PGAs */
  nexrx::TLV320ADC5140::init();

  /* 6. Initialize OSD Audio DMA Data Capture & DSP Engine */
  nexrx::OSDCapture::init();

  /* 7. Initialize Control Plane Transport */
  static nexrx::USBCDCTransport controlTransport;
  controlTransport.init();

  LOG_INF("System Initialization Complete. Automatic power hardware ready.");
  if (nexrx::CPLDDriver::isConfigured()) {
    nexrx::DisplayManager::showStatus("READY");
  } else {
    nexrx::DisplayManager::showStatus("WAIT CPLD");
  }

  while (true) {
    /* Process incoming control messages */
    nexrx::ControlHandler::instance().process(controlTransport);
    k_sleep(K_MSEC(10));
  }

  return 0;
}
