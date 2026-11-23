#pragma once

#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/drivers/pwm.h>

namespace nexrx {

class LEDManager {
public:
  enum class BootStage {
    PowerOn,
    CPLDConfig,
    Si5351Setup,
    USBConfig,
    Ready,
    Count
  };

  struct ColorRGB {
    uint8_t r;
    uint8_t g;
    uint8_t b;
  };

  struct StageConfig {
    BootStage stage;
    const char* name;
    ColorRGB color;
  };

  static LEDManager& instance();

  void init();
  void setStage(BootStage stage);
  void setColor(uint8_t r, uint8_t g, uint8_t b);
  void setBreathCycleMS(uint32_t cycleMS);
  void setBreathingEnabled(bool enabled);

  // Singleton method callbacks
  void onPowerOn() { setStage(BootStage::PowerOn); }
  void onCPLDConfig() { setStage(BootStage::CPLDConfig); }
  void onSi5351Setup() { setStage(BootStage::Si5351Setup); }
  void onUSBConfig() { setStage(BootStage::USBConfig); }
  void onReady() { setStage(BootStage::Ready); }

  BootStage getCurrentStage() const { return currentStage; }
  const char* getCurrentStageName() const;

private:
  struct WorkContext {
    struct k_work work;
    LEDManager* owner;
  };

  LEDManager();
  ~LEDManager() = default;
  LEDManager(const LEDManager&) = delete;
  LEDManager& operator=(const LEDManager&) = delete;

  static void timerExpiryHandler(struct k_timer* timer);
  static void workHandler(struct k_work* work);

  void updateBreathing();
  void applyOutputs(uint32_t lightness);
  void setChannelDuty(const struct pwm_dt_spec& spec, uint8_t channelValue, uint32_t lightness);

  BootStage currentStage;
  atomic_t packedColor;
  atomic_t breathCycleMS;
  atomic_t breathingEnabled;
  bool isInitialized;

  struct k_timer breathTimer;
  WorkContext updateWorkCtx;
};

} // namespace nexrx
