#include "FrontEndDriver.hpp"
#include <zephyr/logging/log.h>
#include <algorithm>

LOG_MODULE_REGISTER(frontend_driver, LOG_LEVEL_INF);

namespace nexrx {

namespace {

const struct gpio_dt_spec atten3Gpio  = GPIO_DT_SPEC_GET_OR(DT_NODELABEL(atten_switches), atten_3db_gpios, {0});
const struct gpio_dt_spec atten6Gpio  = GPIO_DT_SPEC_GET_OR(DT_NODELABEL(atten_switches), atten_6db_gpios, {0});
const struct gpio_dt_spec atten12Gpio = GPIO_DT_SPEC_GET_OR(DT_NODELABEL(atten_switches), atten_12db_gpios, {0});
const struct gpio_dt_spec atten24Gpio = GPIO_DT_SPEC_GET_OR(DT_NODELABEL(atten_switches), atten_24db_gpios, {0});

const struct gpio_dt_spec bpf1Gpio = GPIO_DT_SPEC_GET_OR(DT_NODELABEL(bpf_switches), bpf1_gpios, {0});
const struct gpio_dt_spec bpf2Gpio = GPIO_DT_SPEC_GET_OR(DT_NODELABEL(bpf_switches), bpf2_gpios, {0});
const struct gpio_dt_spec bpf3Gpio = GPIO_DT_SPEC_GET_OR(DT_NODELABEL(bpf_switches), bpf3_gpios, {0});
const struct gpio_dt_spec bpf4Gpio = GPIO_DT_SPEC_GET_OR(DT_NODELABEL(bpf_switches), bpf4_gpios, {0});
const struct gpio_dt_spec hpfGpio  = GPIO_DT_SPEC_GET_OR(DT_NODELABEL(bpf_switches), hpf_gpios, {0});
const struct gpio_dt_spec nhpfGpio = GPIO_DT_SPEC_GET_OR(DT_NODELABEL(bpf_switches), nhpf_gpios, {0});

const struct gpio_dt_spec pwr3V3Gpio = GPIO_DT_SPEC_GET_OR(DT_NODELABEL(power_control), enable_3v3_gpios, {0});

} // namespace

int FrontEndDriver::currentAttenDB = 0;
int FrontEndDriver::currentBpfIndex = 0;
bool FrontEndDriver::currentHpfBypass = false;

void FrontEndDriver::init() {
  LOG_INF("Initializing Front-End Switch Driver (Attenuators & BPF)...");

  // Configure Attenuator GPIOs
  if (device_is_ready(atten3Gpio.port))  gpio_pin_configure_dt(&atten3Gpio, GPIO_OUTPUT_INACTIVE);
  if (device_is_ready(atten6Gpio.port))  gpio_pin_configure_dt(&atten6Gpio, GPIO_OUTPUT_INACTIVE);
  if (device_is_ready(atten12Gpio.port)) gpio_pin_configure_dt(&atten12Gpio, GPIO_OUTPUT_INACTIVE);
  if (device_is_ready(atten24Gpio.port)) gpio_pin_configure_dt(&atten24Gpio, GPIO_OUTPUT_INACTIVE);

  // Configure BPF and HPF GPIOs
  if (device_is_ready(bpf1Gpio.port)) gpio_pin_configure_dt(&bpf1Gpio, GPIO_OUTPUT_INACTIVE);
  if (device_is_ready(bpf2Gpio.port)) gpio_pin_configure_dt(&bpf2Gpio, GPIO_OUTPUT_INACTIVE);
  if (device_is_ready(bpf3Gpio.port)) gpio_pin_configure_dt(&bpf3Gpio, GPIO_OUTPUT_INACTIVE);
  if (device_is_ready(bpf4Gpio.port)) gpio_pin_configure_dt(&bpf4Gpio, GPIO_OUTPUT_INACTIVE);
  if (device_is_ready(hpfGpio.port))  gpio_pin_configure_dt(&hpfGpio, GPIO_OUTPUT_ACTIVE);
  if (device_is_ready(nhpfGpio.port)) gpio_pin_configure_dt(&nhpfGpio, GPIO_OUTPUT_INACTIVE);

  // Configure Power GPIO
  if (device_is_ready(pwr3V3Gpio.port)) {
    gpio_pin_configure_dt(&pwr3V3Gpio, GPIO_OUTPUT_ACTIVE);
    set3V3Power(true);
  }

  setAttenuation(0);
  setHpfBypass(false);
  LOG_INF("Front-End Switch Driver Initialized.");
}

int FrontEndDriver::setAttenuation(int attenDB) {
  int clamped = std::clamp(attenDB, 0, 45);
  int steps = clamped / 3;
  int rounded = steps * 3;

  int remaining = rounded;
  bool use24 = (remaining >= 24);
  if (use24) remaining -= 24;

  bool use12 = (remaining >= 12);
  if (use12) remaining -= 12;

  bool use6 = (remaining >= 6);
  if (use6) remaining -= 6;

  bool use3 = (remaining >= 3);
  if (use3) remaining -= 3;

  if (device_is_ready(atten24Gpio.port)) gpio_pin_set_dt(&atten24Gpio, use24 ? 1 : 0);
  if (device_is_ready(atten12Gpio.port)) gpio_pin_set_dt(&atten12Gpio, use12 ? 1 : 0);
  if (device_is_ready(atten6Gpio.port))  gpio_pin_set_dt(&atten6Gpio, use6 ? 1 : 0);
  if (device_is_ready(atten3Gpio.port))  gpio_pin_set_dt(&atten3Gpio, use3 ? 1 : 0);

  currentAttenDB = rounded;
  LOG_DBG("Front-End Attenuation set to %d dB (24:%d, 12:%d, 6:%d, 3:%d)",
          rounded, use24, use12, use6, use3);
  return rounded;
}

void FrontEndDriver::selectFilterBand(uint32_t freqHz) {
  // AM Broadcast HPF cutoff is 1.75 MHz
  bool bypassHpf = (freqHz < 1750000);
  setHpfBypass(bypassHpf);

  // 4 Octave BPF banks
  int bpf = 0;
  if (freqHz >= 1800000 && freqHz < 3400000) {
    bpf = 1;
  } else if (freqHz >= 3200000 && freqHz < 7500000) {
    bpf = 2;
  } else if (freqHz >= 7300000 && freqHz < 14500000) {
    bpf = 3;
  } else if (freqHz >= 14300000 && freqHz <= 30000000) {
    bpf = 4;
  } else {
    // Outside direct amateur octaves: select nearest
    if (freqHz < 1800000) bpf = 1;
    else bpf = 4;
  }

  setBpfIndex(bpf);
}

void FrontEndDriver::setBpfIndex(int bpfIndex) {
  currentBpfIndex = bpfIndex;

  if (device_is_ready(bpf1Gpio.port)) gpio_pin_set_dt(&bpf1Gpio, (bpfIndex == 1) ? 1 : 0);
  if (device_is_ready(bpf2Gpio.port)) gpio_pin_set_dt(&bpf2Gpio, (bpfIndex == 2) ? 1 : 0);
  if (device_is_ready(bpf3Gpio.port)) gpio_pin_set_dt(&bpf3Gpio, (bpfIndex == 3) ? 1 : 0);
  if (device_is_ready(bpf4Gpio.port)) gpio_pin_set_dt(&bpf4Gpio, (bpfIndex == 4) ? 1 : 0);

  LOG_DBG("Front-End BPF branch selected: %d", bpfIndex);
}

void FrontEndDriver::setHpfBypass(bool bypass) {
  currentHpfBypass = bypass;

  // Active high hpf enables filter, nhpf is inverted
  if (device_is_ready(hpfGpio.port))  gpio_pin_set_dt(&hpfGpio, bypass ? 0 : 1);
  if (device_is_ready(nhpfGpio.port)) gpio_pin_set_dt(&nhpfGpio, bypass ? 1 : 0);

  LOG_DBG("Front-End HPF %s", bypass ? "BYPASSED" : "ENABLED");
}

void FrontEndDriver::set3V3Power(bool enable) {
  if (device_is_ready(pwr3V3Gpio.port)) {
    gpio_pin_set_dt(&pwr3V3Gpio, enable ? 1 : 0);
    LOG_INF("Front-End 3.3V Power Rail %s", enable ? "ENABLED" : "DISABLED");
  }
}

int FrontEndDriver::getAttenuation() {
  return currentAttenDB;
}

int FrontEndDriver::getBpfIndex() {
  return currentBpfIndex;
}

bool FrontEndDriver::isHpfBypassed() {
  return currentHpfBypass;
}

} // namespace nexrx
