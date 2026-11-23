#include "DisplayManager.hpp"
#include "FrameBuffer.h"

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/cache.h>
#include <zephyr/linker/section_tags.h>
#include <string.h>

LOG_MODULE_DECLARE(nexrx_main, LOG_LEVEL_INF);

extern const FrameBuffer::Font UbuntuB16;
extern const FrameBuffer::Font UbuntuM13;
extern const FrameBuffer::Font PlexL12;

namespace nexrx {

const struct device* DisplayManager::displayDev = nullptr;
struct k_mutex DisplayManager::displayMutex;

static uint8_t __nocache fbBits[FrameBuffer::FBSIZE] __aligned(32);
static FrameBuffer fb(fbBits);

void DisplayManager::init() {
  k_mutex_init(&displayMutex);
  displayDev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

  if (!device_is_ready(displayDev)) {
    LOG_ERR("Display: UC1609 device not ready");
    return;
  }

  LOG_INF("Display: UC1609 initialized (%dx%d)", FrameBuffer::WIDTH, FrameBuffer::HEIGHT);
  clear();
  drawSplash();
}

void DisplayManager::update() {
  if (!displayDev || !device_is_ready(displayDev)) {
    return;
  }

  struct display_buffer_descriptor desc = {
    .buf_size = FrameBuffer::FBSIZE,
    .width = FrameBuffer::WIDTH,
    .height = FrameBuffer::HEIGHT,
    .pitch = FrameBuffer::WIDTH,
  };

  sys_cache_data_flush_range(fbBits, FrameBuffer::FBSIZE);
  display_write(displayDev, 0, 0, &desc, fbBits);
}

void DisplayManager::clear() {
  k_mutex_lock(&displayMutex, K_FOREVER);
  memset(fbBits, 0, FrameBuffer::FBSIZE);
  update();
  k_mutex_unlock(&displayMutex);
}

void DisplayManager::drawSplash() {
  k_mutex_lock(&displayMutex, K_FOREVER);
  memset(fbBits, 0, FrameBuffer::FBSIZE);

  // Title: "NexRx SDR" at top (y=0, occupies pages 0 and 1)
  fb.drawString("NexRx SDR", 4, 0, UbuntuB16);

  // Horizontal divider line at y=18
  fb.drawFilledRect(0, 18, FrameBuffer::WIDTH, 1);

  // Subtitle / tag at bottom (y=48, occupies pages 6 and 7)
  fb.drawString("HF SDR Receiver", 4, 48, PlexL12);

  update();
  k_mutex_unlock(&displayMutex);
}

void DisplayManager::showStatus(const char* status) {
  if (!status || !displayDev) {
    return;
  }

  k_mutex_lock(&displayMutex, K_FOREVER);

  // Erase status region (y=20 to y=46)
  fb.eraseRect(0, 20, FrameBuffer::WIDTH, 26);

  // Draw status message at page-aligned y=24 (occupies pages 3 and 4)
  fb.drawString(status, 4, 24, UbuntuM13);

  update();
  k_mutex_unlock(&displayMutex);

  LOG_INF("Display Status: %s", status);
}

} // namespace nexrx
