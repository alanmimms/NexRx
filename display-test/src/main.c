#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>
#include <zephyr/display/cfb.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/sys/timeutil.h>
#include <zephyr/sys/time_units.h>
#include <zephyr/kernel.h>
#include "cfb_font_12_7x16.h"
#include <time.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

static const int fontH = 16;
static const int fontW = 7;

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));


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



// Returns the font index to be used with cfb_framebuffer_set_font().
static int configureFont(const struct device *d, int w, int h) {
  int nFonts = cfb_get_numof_fonts(d);

  for (int fontX = 0; fontX < nFonts; ++fontX) {
    uint8_t thisW, thisH;

    if (cfb_get_font_size(d, fontX, &thisW, &thisH) == 0) {

      if (w == thisW && h == thisH) {
        cfb_framebuffer_set_font(d, fontX);
	return fontX;
      }
    }
  }

  LOG_ERR("configureFont failed to find font with w=%d h=%d", (int) w, (int) h);
  return 0;
}


static void drawSMeter(int dB) {
  (void) cfb_draw_text(display, "S0 3 9", 0*fontW, 0*fontH);

  static const int fastH = (fontH - 1)/2;
  static const int peakH = fastH;
  static const int midH = fontH/2;
  static const int MAX_SMETER_DB = 24;
  static const int sMeterW = fontW * 12;
  static const int decayDelta = 3;
#define DB_TO_X(DB)	((DB) * sMeterW / MAX_SMETER_DB)

  if (dB > MAX_SMETER_DB) dB = MAX_SMETER_DB;

  // Show current value without any decay
  int w = DB_TO_X(dB);
  (void) cfb_invert_area(display, 0, 1*fontH + + midH - fastH/2, w, fastH);

  // Show peak hold value with slowish decay
  static int lastPeak;
  int peakdB = dB;

  // Handle decay
  if (dB < lastPeak && dB > 0) peakdB = lastPeak - decayDelta;

  lastPeak = peakdB;

  w = DB_TO_X(peakdB);
  struct cfb_position startR = {.x = 0, .y = 1*fontH + midH - peakH/2};
  struct cfb_position endR = {.x = w, .y = startR.y + peakH};
  (void) cfb_draw_rect(display, &startR, &endR);
}


static void drawDateTime(struct tm *timeP) {
  char buf[32];
  strftime(buf, sizeof(buf), "%H:%M:%S", timeP);
  (void) cfb_draw_text(display, buf, 15*fontW, 0*fontH);
  strftime(buf, sizeof(buf), "%a %Y-%m-%d", timeP);
  (void) cfb_draw_text(display, buf, 13*fontW, 1*fontH);
}


static void drawFreq(const int freq) {
  char buf[32];
  sprintf(buf, "%s %2d,%03d,%03d", "USB", freq/1000000, (freq/1000) % 1000, freq % 1000);
  (void) cfb_draw_text(display, buf, 5*fontW, 2*fontH);
}


static void drawUSBStatus(bool up) {
  (void) cfb_draw_text(display, up ? "[USB data connected]" : "[NO USB data linked!]", 0*fontW, 3*fontH);
}


int main(void) {
  int st;

  LOG_INF("Serial console initialized.");

  if (!device_is_ready(display)) {
    LOG_ERR("Display device is NOT ready.");
    return -ENODEV;
  }

  LOG_INF("Display %s initialized.", display->name);
  display_blanking_off(display);

  struct display_capabilities caps;
  display_get_capabilities(display, &caps);
  LOG_INF("Resolution: %dx%d.", caps.x_resolution, caps.y_resolution);

  st = cfb_framebuffer_init(display);
  LOG_INF("Framebuffer initialized %d.", st);

  int fontX = configureFont(display, fontW, fontH);
  LOG_INF("Font index %d.", fontX);
  cfb_framebuffer_set_font(display, fontX);

#define NITERS 10000
  uint64_t startT = k_cycle_get_64();

  static struct tm startTM = {
    .tm_sec = 0,
    .tm_min = 59,
    .tm_hour = 23,
    .tm_mday = 23,
    .tm_mon = 10,
    .tm_year = 2026 - 1900,
    .tm_wday = 0,
    .tm_yday = 0,
    .tm_isdst = 0,
  };

  time_t startTime = tmToEpochUTC(&startTM);

  for (int k=NITERS; k--; ) {
    time_t now = startTime + k_cyc_to_sec_floor64(k_cycle_get_64() - startT);
    struct tm nowTM;
    gmtime_r(&now, &nowTM);

    (void) cfb_framebuffer_clear(display, true);
    int dB = rand() % 30 + 1;
    drawSMeter(dB);
    drawDateTime(&nowTM);
    drawFreq(12345678);
    drawUSBStatus(true);

    cfb_framebuffer_finalize(display);
    k_msleep(200);
  }

  uint64_t endT = k_cycle_get_64();
  LOG_INF("Time for %d iterations: %ums.", NITERS, (unsigned) k_cyc_to_ms_near64(endT - startT));

  st = cfb_framebuffer_finalize(display);
  LOG_INF("Framebuffer finalized %d.", st);

  return 0;
}
