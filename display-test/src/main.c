#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/display/cfb.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/sys/printk.h>
#include <zephyr/kernel.h>


#define OLED_NODE DT_NODELABEL(ssd1306)

int main(void) {
  const struct device *display = DEVICE_DT_GET(DT_NODELABEL(ssd1306));
  int st;

  const struct device *csPort  = DEVICE_DT_GET(DT_SPI_DEV_CS_GPIOS_CTLR(OLED_NODE));
  const struct device *dcPort  = DEVICE_DT_GET(DT_GPIO_CTLR(OLED_NODE, data_cmd_gpios));
  const struct device *resPort = DEVICE_DT_GET(DT_GPIO_CTLR(OLED_NODE, reset_gpios));

  if (!device_is_ready(csPort) || !device_is_ready(dcPort) || !device_is_ready(resPort)) {
    printk("Error: One or more GPIO ports are not ready.\n");
    return -1;
  }  

  printk("Serial console initialized.\n");

  if (!device_is_ready(display)) {
    return -1;
  }

  printk("Display device is ready\n");

  st = cfb_framebuffer_init(display);
  printk("Framebuffer initialized %d\n", st);

  gpio_pin_t csPin  = DT_SPI_DEV_CS_GPIOS_PIN(OLED_NODE);
  gpio_pin_t dcPin  = DT_GPIO_PIN(OLED_NODE, data_cmd_gpios);
  gpio_pin_t resPin = DT_GPIO_PIN(OLED_NODE, reset_gpios);

  int nss = gpio_pin_get_raw(csPort, csPin);
  int dc  = gpio_pin_get_raw(dcPort, dcPin);
  int res = gpio_pin_get_raw(resPort, resPin);
  printk("Early GPIO pin states: NSS=%d DC=%d RES=%d\n", nss, dc, res);

  uint64_t startT = k_cycle_get_64();

#define MAYBE_PRINTK while(0)
  #define NITERS 1000

  for (int k=NITERS; --k; ) {
    st = cfb_framebuffer_clear(display, true);
    MAYBE_PRINTK("Framebuffer cleared %d\n", st);

    static char msg[] =
      "0123456789AB"
      " Line2      "
      "  Line3     "
      "   Line4    ";
    
    st = cfb_print(display, msg, 0, 0);
    MAYBE_PRINTK("Framebuffer string printed %d\n", st);

    static const struct cfb_position cirPos = {64, 32};
  
    st = cfb_draw_circle(display, &cirPos, 31);
    MAYBE_PRINTK("Framebuffer draw circle %d\n", st);

    int h = cfb_get_display_parameter(display, CFB_DISPLAY_HEIGHT);
    int w = cfb_get_display_parameter(display, CFB_DISPLAY_WIDTH);
    int ppt = cfb_get_display_parameter(display, CFB_DISPLAY_PPT);
    int rows = cfb_get_display_parameter(display, CFB_DISPLAY_ROWS);
    int cols = cfb_get_display_parameter(display, CFB_DISPLAY_COLS);
    int nFonts = cfb_get_numof_fonts(display);
    MAYBE_PRINTK("Framebuffer h=%d w=%d ppt=%d rows=%d cols=%d nFonts=%d\n", h, w, ppt, rows, cols, nFonts);
  }

  uint64_t endT = k_cycle_get_64();
  printk("Time for %d iterations: %ums\n", NITERS, (unsigned) k_cyc_to_ms_near64(endT - startT));

  st = cfb_framebuffer_finalize(display);
  printk("Framebuffer finalized %d\n", st);

  nss = gpio_pin_get_raw(csPort, csPin);
  dc  = gpio_pin_get_raw(dcPort, dcPin);
  res = gpio_pin_get_raw(resPort, resPin);
  printk("Ending GPIO pin states: NSS=%d DC=%d RES=%d\n", nss, dc, res);

  return 0;
}
