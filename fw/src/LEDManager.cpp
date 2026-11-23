#include "LEDManager.hpp"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(led_manager, LOG_LEVEL_INF);

namespace nexrx {

namespace {

constexpr LEDManager::StageConfig stageTable[] = {
  { LEDManager::BootStage::PowerOn,     "PowerOn",     { 255,   0,   0 } }, /* Red */
  { LEDManager::BootStage::CPLDConfig,  "CPLDConfig",  { 255,  60,   0 } }, /* Orange */
  { LEDManager::BootStage::Si5351Setup, "Si5351Setup", { 255, 200,   0 } }, /* Yellow */
  { LEDManager::BootStage::USBConfig,   "USBConfig",   {   0, 200, 200 } }, /* Teal */
  { LEDManager::BootStage::Ready,       "Ready",       {   0, 255,   0 } }, /* Green */
};

constexpr int stageCount = static_cast<int>(sizeof(stageTable) / sizeof(stageTable[0]));

const struct pwm_dt_spec redPWM   = PWM_DT_SPEC_GET(DT_NODELABEL(red_pwm_led));
const struct pwm_dt_spec greenPWM = PWM_DT_SPEC_GET(DT_NODELABEL(green_pwm_led));
const struct pwm_dt_spec bluePWM  = PWM_DT_SPEC_GET(DT_NODELABEL(blue_pwm_led));

constexpr uint32_t msPerSecond = 1000U;
constexpr uint32_t updateRateHz = 100U;
constexpr uint32_t updatePeriodMS = msPerSecond / updateRateHz;
constexpr uint32_t defaultBreathCycleMS = 5U * msPerSecond;

constexpr uint32_t maxChannelValue = 255U;
constexpr uint32_t maxLightness = 65535U;
constexpr uint32_t minLightnessFloor8Bit = 5U;
constexpr uint32_t minLightness = (minLightnessFloor8Bit * maxLightness) / maxChannelValue;
constexpr uint64_t maxDutyProduct = static_cast<uint64_t>(maxChannelValue) * maxLightness;

constexpr int lightnessTableBits = 8;
constexpr int lightnessTableSize = 1 << lightnessTableBits;
constexpr uint32_t lightnessTableMask = static_cast<uint32_t>(lightnessTableSize - 1);
constexpr uint32_t interpFracBits = 8U;
constexpr uint32_t interpFracScale = 1U << interpFracBits;
constexpr uint32_t interpFracMask = interpFracScale - 1U;

constexpr double pi = 3.14159265358979323846;

/* Portable compile-time cosine using range reduction to [0, pi/2] and 5-term Taylor series */
constexpr double cosApprox(double x) {
  double sign = 1.0;
  if (x > pi) {
    x = (2.0 * pi) - x;
  }
  if (x > (pi * 0.5)) {
    x = pi - x;
    sign = -1.0;
  }

  double x2 = x * x;
  double term = 1.0;
  double sum = 1.0;
  for (int n = 1; n <= 5; ++n) {
    term *= -x2 / static_cast<double>((2 * n - 1) * (2 * n));
    sum += term;
  }
  return sign * sum;
}

struct LightnessLUT {
  uint16_t entries[lightnessTableSize];
};

/* Compile-time raised-cosine perceptual lightness table (gamma ~2.0) */
constexpr LightnessLUT buildLightnessTable() {
  LightnessLUT lut{};
  constexpr double range = static_cast<double>(maxLightness - minLightness);
  for (int i = 0; i < lightnessTableSize; ++i) {
    double theta = (2.0 * pi * static_cast<double>(i)) / static_cast<double>(lightnessTableSize);
    double raisedCos = 0.5 * (1.0 - cosApprox(theta));
    double perceptual = raisedCos * raisedCos;
    lut.entries[i] = static_cast<uint16_t>(static_cast<double>(minLightness) + (range * perceptual) + 0.5);
  }
  return lut;
}

constexpr LightnessLUT lightnessTable = buildLightnessTable();

constexpr uint32_t packRGB(uint8_t r, uint8_t g, uint8_t b) {
  return (static_cast<uint32_t>(r) << 16) |
         (static_cast<uint32_t>(g) << 8) |
         static_cast<uint32_t>(b);
}

} // namespace

LEDManager& LEDManager::instance() {
  static LEDManager inst;
  return inst;
}

LEDManager::LEDManager()
  : currentStage(BootStage::PowerOn),
    packedColor(ATOMIC_INIT(static_cast<atomic_val_t>(packRGB(255U, 0U, 0U)))),
    breathCycleMS(ATOMIC_INIT(static_cast<atomic_val_t>(defaultBreathCycleMS))),
    breathingEnabled(ATOMIC_INIT(1)),
    isInitialized(false),
    breathTimer{},
    updateWorkCtx{{}, this} {
}

const char* LEDManager::getCurrentStageName() const {
  for (int i = 0; i < stageCount; ++i) {
    if (stageTable[i].stage == currentStage) {
      return stageTable[i].name;
    }
  }
  return "Unknown";
}

void LEDManager::init() {
  if (isInitialized) {
    return;
  }

  LOG_INF("Initializing HW PWM RGB Status LED (%u Hz update, %u ms cycle)...",
          updateRateHz,
          static_cast<uint32_t>(atomic_get(&breathCycleMS)));

  if (!pwm_is_ready_dt(&redPWM)) {
    LOG_WRN("Red PWM LED device not ready");
  }
  if (!pwm_is_ready_dt(&greenPWM)) {
    LOG_WRN("Green PWM LED device not ready");
  }
  if (!pwm_is_ready_dt(&bluePWM)) {
    LOG_WRN("Blue PWM LED device not ready");
  }

  updateWorkCtx.owner = this;
  k_work_init(&updateWorkCtx.work, workHandler);

  k_timer_init(&breathTimer, timerExpiryHandler, nullptr);
  k_timer_user_data_set(&breathTimer, this);

  setStage(BootStage::PowerOn);
  k_timer_start(&breathTimer, K_NO_WAIT, K_MSEC(updatePeriodMS));

  isInitialized = true;
}

void LEDManager::setStage(BootStage stage) {
  currentStage = stage;
  const char* stageName = "Unknown";
  for (int i = 0; i < stageCount; ++i) {
    if (stageTable[i].stage == stage) {
      setColor(stageTable[i].color.r, stageTable[i].color.g, stageTable[i].color.b);
      stageName = stageTable[i].name;
      break;
    }
  }

  uint32_t packed = static_cast<uint32_t>(atomic_get(&packedColor));
  LOG_INF("RGB LED Stage -> %s (RGB: %u, %u, %u)",
          stageName,
          (packed >> 16) & 0xFFU,
          (packed >> 8) & 0xFFU,
          packed & 0xFFU);
}

void LEDManager::setColor(uint8_t r, uint8_t g, uint8_t b) {
  atomic_set(&packedColor, static_cast<atomic_val_t>(packRGB(r, g, b)));
}

void LEDManager::setBreathCycleMS(uint32_t cycleMS) {
  if (cycleMS > 0U) {
    atomic_set(&breathCycleMS, static_cast<atomic_val_t>(cycleMS));
  }
}

void LEDManager::setBreathingEnabled(bool enabled) {
  atomic_set(&breathingEnabled, enabled ? 1 : 0);
}

void LEDManager::timerExpiryHandler(struct k_timer* timer) {
  LEDManager* self = static_cast<LEDManager*>(k_timer_user_data_get(timer));
  if (self != nullptr) {
    k_work_submit(&self->updateWorkCtx.work);
  }
}

void LEDManager::workHandler(struct k_work* work) {
  WorkContext* ctx = CONTAINER_OF(work, WorkContext, work);
  if (ctx->owner != nullptr) {
    ctx->owner->updateBreathing();
  }
}

void LEDManager::updateBreathing() {
  uint32_t lightness = maxLightness;

  if (atomic_get(&breathingEnabled) != 0) {
    uint32_t cycleMS = static_cast<uint32_t>(atomic_get(&breathCycleMS));
    uint32_t phaseMS = k_uptime_get_32() % cycleMS;

    /* Fixed-point index into LUT with 8 fractional bits for linear interpolation */
    uint64_t scaledPhase = (static_cast<uint64_t>(phaseMS) *
			    static_cast<uint64_t>(lightnessTableSize) *
			    interpFracScale) / cycleMS;
    uint32_t idx0 = static_cast<uint32_t>(scaledPhase >> interpFracBits) & lightnessTableMask;
    uint32_t idx1 = (idx0 + 1U) & lightnessTableMask;
    int32_t frac = static_cast<int32_t>(scaledPhase & interpFracMask);

    int32_t l0 = static_cast<int32_t>(lightnessTable.entries[idx0]);
    int32_t l1 = static_cast<int32_t>(lightnessTable.entries[idx1]);
    lightness = static_cast<uint32_t>(l0 + (((l1 - l0) * frac) / static_cast<int32_t>(interpFracScale)));
  }

  applyOutputs(lightness);
}

void LEDManager::applyOutputs(uint32_t lightness) {
  uint32_t packed = static_cast<uint32_t>(atomic_get(&packedColor));
  uint8_t baseR = static_cast<uint8_t>((packed >> 16) & 0xFFU);
  uint8_t baseG = static_cast<uint8_t>((packed >> 8) & 0xFFU);
  uint8_t baseB = static_cast<uint8_t>(packed & 0xFFU);

  setChannelDuty(redPWM, baseR, lightness);
  setChannelDuty(greenPWM, baseG, lightness);
  setChannelDuty(bluePWM, baseB, lightness);
}

void LEDManager::setChannelDuty(const struct pwm_dt_spec& spec, uint8_t channelValue, uint32_t lightness) {
  if (!pwm_is_ready_dt(&spec)) {
    return;
  }

  uint64_t pulseNs = (static_cast<uint64_t>(channelValue) * lightness * spec.period) / maxDutyProduct;
  pwm_set_pulse_dt(&spec, static_cast<uint32_t>(pulseNs));
}

} // namespace nexrx
