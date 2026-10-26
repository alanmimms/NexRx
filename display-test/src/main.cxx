#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/sys/timeutil.h>
#include <zephyr/sys/time_units.h>
#include <zephyr/kernel.h>
#include <zephyr/cache.h>
#include <zephyr/linker/devicetree_regions.h>

#include <time.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "FrameBuffer.h"
#include <memory>

extern const FrameBuffer::Font UbuntuM16;

static const int fontH = 16;
static const int fontW = 7;

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

static std::unique_ptr<FrameBuffer> fbP = nullptr;

struct SPIDMABuffers {
  uint8_t fbPing[FrameBuffer::FBSIZE];
  uint8_t fbPong[FrameBuffer::FBSIZE];
  uint8_t cmdBuf[16];
};

#define NOCACHE_SECTION LINKER_DT_NODE_REGION_NAME(DT_NODELABEL(sram1_nocache))
Z_GENERIC_SECTION(NOCACHE_SECTION)  __aligned(4) static SPIDMABuffers dmaBuffers;

static const int daysBeforeMonth[] = {
  0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};

static const int secondsPerHour = 60 * 60;
static const int secondsPerDay  = 24 * (60 * 60);

static bool isLeapYear(int year) {
  return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

// Portable UTC converter from struct tm to epoch seconds
static time_t tmToEpochUTC(const struct tm *timeP) {
  int year = timeP->tm_year + 1900;
  int month = timeP->tm_mon; // 0-indexed [0, 11]

  int numLeapYears = 0;

  for (int y = 1970; y < year; ++y) {
    if (isLeapYear(y)) numLeapYears++;
  }

  int days = (year - 1970) * 365 + numLeapYears + daysBeforeMonth[month] + (timeP->tm_mday - 1);
  if (month > 1 && isLeapYear(year)) days++;

  time_t totalSeconds = (time_t)days * secondsPerDay;
  totalSeconds += (time_t)timeP->tm_hour * secondsPerHour;
  totalSeconds += (time_t)timeP->tm_min * 60;
  totalSeconds += (time_t)timeP->tm_sec;

  return totalSeconds;
}

static void drawSMeter(int dB) {
  static const int sMeterLine = 0;
  static const int fastH = (fontH - 1)/2;
  static const int peakH = fastH;
  static const int midH = fontH/2;
  static const int MAX_SMETER_DB = 24;
  static const int sMeterW = (27 - 8) * fontW - 2;
  static const int decayDelta = 3;
#define DB_TO_X(DB)	((DB) * sMeterW / MAX_SMETER_DB)

  //                              0123456789
  fbP->drawString("S0==3=====9", 0*fontW, (sMeterLine+1)*fontH, UbuntuM16);

  if (dB > MAX_SMETER_DB) dB = MAX_SMETER_DB;

  // Show current value without any decay
  unsigned w = DB_TO_X(dB);
  unsigned y = sMeterLine*fontH + midH - fastH/2;
  fbP->invertRect(0, y, w, fastH);

  // Show peak hold value with slowish decay
  static int lastPeak;
  int peakdB = dB;

  // Handle decay
  if (dB < lastPeak && dB > 0) peakdB = lastPeak - decayDelta;

  lastPeak = peakdB;

  w = DB_TO_X(peakdB);
  fbP->drawFilledRect(0, sMeterLine*fontH + midH - peakH/2, w, y + peakH);
}


static void drawDateTime(struct tm *timeP) {
  char buf[32];
  strftime(buf, sizeof(buf), "%H:%M:%S", timeP);
  fbP->drawString(buf, 19*fontW, 0*fontH, UbuntuM16);
  strftime(buf, sizeof(buf), "%a %Y-%m-%d", timeP);
  fbP->drawString(buf, 13*fontW, 1*fontH, UbuntuM16);
}


static void drawFreq(const int freq) {
  char buf[32];
  sprintf(buf, "%s %2d,%03d,%03d", "USB", freq/1000000, (freq/1000) % 1000, freq % 1000);
  fbP->drawString(buf, 5*fontW, 2*fontH, UbuntuM16);
}


static void drawUSBStatus(bool up) {
  fbP->drawString(up ? "[USB]" : "[NO USB]", 0*fontW, 3*fontH, UbuntuM16);
}


static void updateFB() {
  const struct display_driver_api *api = static_cast<const struct display_driver_api *>(display->api);
  struct display_buffer_descriptor desc = {
    .buf_size = fbP->sizeBytes,
    .width = fbP->width,
    .height = fbP->height,
    .pitch = fbP->width,
  };

  sys_cache_data_flush_range(fbP->bitsP, FrameBuffer::FBSIZE);    
  api->write(display, 0, 0, &desc, fbP->bitsP);
}


int main(void) {
  LOG_INF("Serial console initialized.");

  if (!device_is_ready(display)) {
    LOG_ERR("Display device is NOT ready.");
    return -ENODEV;
  }

  LOG_INF("Display %s initialized.", display->name);

  struct display_capabilities caps;
  display_get_capabilities(display, &caps);
  LOG_INF("Resolution: %dx%d.", caps.x_resolution, caps.y_resolution);

  fbP = std::make_unique<FrameBuffer>(&dmaBuffers.fbPing[0]);

  uint64_t startT = k_cycle_get_64();

  static struct tm startTM = {
    .tm_sec = 0,
    .tm_min = 59,
    .tm_hour = 23,
    .tm_mday = 23,
    .tm_mon = 10 - 1,
    .tm_year = 2026 - 1900,
    .tm_wday = 5,
    .tm_yday = 295,
    .tm_isdst = 1,
  };

  printk("Font hPages=%u, bitsP=%p\n", UbuntuM16.hPages, UbuntuM16.bitsP);
  const FrameBuffer::Glyph &g = UbuntuM16.glyphs[42];
  printk("  Glyphs[42]: .bitOffset=%u, .inkW=%u, .advance=%u\n", g.bitOffset, g.inkW, g.advance);
  printk("fbP->bitsP=%p\n", fbP->bitsP);

  memset(fbP->bitsP, 0xC3, fbP->FBSIZE);
  fbP->drawFilledRect(17, 11, 122 - 17, 37 - 11);
  printk("Back from drawFilledRect\n");
#if 0
  for (unsigned page=3; page < 7; ++page) {
    for (unsigned x=0; x < 192; ++x) fbP->bitsP[x + page*192] = x;
  }
#endif

  updateFB();
  printk("Back from first updateFB\n");

  time_t startTime = tmToEpochUTC(&startTM);

  return 0;

  for (;;) {
    printk("Tick\n");
    fbP->eraseRect();

    int dB = rand() % 30 + 1;
    drawSMeter(dB);

    time_t now = startTime + k_cyc_to_sec_floor64(k_cycle_get_64() - startT);
    struct tm nowTM;
    gmtime_r(&now, &nowTM);
    drawDateTime(&nowTM);

    drawFreq(12345678);

    drawUSBStatus(true);

    updateFB();
    k_msleep(30000);
  }

  return 0;
}
