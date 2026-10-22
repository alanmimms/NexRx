#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>
#include <zephyr/display/cfb.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/kernel.h>
#include "cfb_font_12_7x16.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));


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

  int fontX = configureFont(display, 6, 16);
  LOG_INF("Font index %d.", fontX);
  cfb_framebuffer_set_font(display, fontX);

  uint64_t startT = k_cycle_get_64();

  //#define MAYBE_LOG while(0) LOG_INF
#define MAYBE_LOG LOG_INF
#define NITERS 1

  for (int k=NITERS; k--; ) {
    st = cfb_framebuffer_clear(display, true);
    MAYBE_LOG("Framebuffer cleared %d.", st);

    static const int fontH = 16;

    st = cfb_print(display, "0123456789AB", 0, 0*fontH);
    MAYBE_LOG("Framebuffer string printed %d.", st);

    st = cfb_print(display, " Line2", 0, 1*fontH);
    MAYBE_LOG("Framebuffer string printed %d.", st);

    st = cfb_print(display, "  Line3", 0, 2*fontH);
    MAYBE_LOG("Framebuffer string printed %d.", st);

    st = cfb_print(display, "   Line4", 0, 3*fontH);
    MAYBE_LOG("Framebuffer string printed %d.", st);

    static const struct cfb_position cirPos = {192/2, 32};
  
    st = cfb_draw_circle(display, &cirPos, 31);
    MAYBE_LOG("Framebuffer draw circle %d.", st);

    int h = cfb_get_display_parameter(display, CFB_DISPLAY_HEIGHT);
    int w = cfb_get_display_parameter(display, CFB_DISPLAY_WIDTH);
    int ppt = cfb_get_display_parameter(display, CFB_DISPLAY_PPT);
    int rows = cfb_get_display_parameter(display, CFB_DISPLAY_ROWS);
    int cols = cfb_get_display_parameter(display, CFB_DISPLAY_COLS);
    int nFonts = cfb_get_numof_fonts(display);
    MAYBE_LOG("Framebuffer h=%d w=%d ppt=%d rows=%d cols=%d nFonts=%d.", h, w, ppt, rows, cols, nFonts);
  }

  uint64_t endT = k_cycle_get_64();
  LOG_INF("Time for %d iterations: %ums.", NITERS, (unsigned) k_cyc_to_ms_near64(endT - startT));

  st = cfb_framebuffer_finalize(display);
  LOG_INF("Framebuffer finalized %d.", st);

  return 0;
}
