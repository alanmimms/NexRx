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


static const struct gpio_dt_spec usbPHYReset =
    GPIO_DT_SPEC_GET(DT_NODELABEL(usbhs_ulpi_phy), reset_gpios);

constexpr int resetHoldMs = 5;
constexpr int pllLockDelayMs = 10;

static int resetUSBPHY() {
  LOG_ERR("resetUSBPHY start");

  bool hseReady = (RCC->CR & RCC_CR_HSERDY) != 0U;
  static constexpr uint32_t mco2SourceHSE = (2U << RCC_CFGR_MCO2_Pos);
  uint32_t mco2Source = RCC->CFGR & RCC_CFGR_MCO2_Msk;
  uint32_t mco2Pre = (RCC->CFGR & RCC_CFGR_MCO2PRE_Msk) >> RCC_CFGR_MCO2PRE_Pos;

  LOG_ERR("check MCO2 output");
  if (!hseReady || mco2Source != mco2SourceHSE || mco2Pre == 0U) {
    LOG_ERR("USB clock output MCO2 not ready");
    return -EIO;
  }

  if (!gpio_is_ready_dt(&usbPHYReset)) {
    LOG_ERR("USB reset GPIO not ready");
    return -ENODEV;
  }

  LOG_ERR("resetUSBPHY assert");

  // Assert /RESET (drive PC4 low because of GPIO_ACTIVE_LOW)
  int err = gpio_pin_configure_dt(&usbPHYReset, GPIO_OUTPUT_ACTIVE);
  if (err != 0) {
    LOG_ERR("Failed to assert USB PHY reset: %d", err);
    return err;
  }
  k_msleep(resetHoldMs);

  LOG_ERR("resetUSBPHY deassert");

  // Deassert /RESET (drive PC4 high) and wait for 60 MHz CLKOUT to stabilize
  err = gpio_pin_set_dt(&usbPHYReset, 0);
  if (err != 0) {
    LOG_ERR("Failed to deassert USB PHY reset: %d", err);
    return err;
  }
  k_msleep(pllLockDelayMs);

  LOG_INF("USB PHY reset pulse complete, PC4 deasserted (3.3V)");
  LOG_ERR("resetUSBPHY done");
  return 0;
}

SYS_INIT(resetUSBPHY, POST_KERNEL, 45);



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
