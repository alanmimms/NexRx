#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/display/cfb.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/sys/printk.h>

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

  // Initialize the USB subsystem
  if (usb_enable(NULL)) {
    return -1;
  }

  // Optional: Wait for a terminal to connect before proceeding
  // k_sleep(K_MSEC(2000)); 
    
  printk("USB Console initialized.\n");

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

  st = cfb_framebuffer_clear(display, true);
  printk("Framebuffer cleared %d\n", st);

  st = cfb_print(display, "NexRx OLED Test", 0, 0);
  printk("Framebuffer string printed %d\n", st);

  static const struct cfb_position cirPos = {64, 32};
  
  st = cfb_draw_circle(display, &cirPos, 13);
  printk("Framebuffer draw circle %d\n", st);

  st = cfb_invert_area(display, 3, 3, 123, 61);
  printk("Framebuffer invert area %d\n", st);

  st = cfb_framebuffer_finalize(display);
  printk("Framebuffer finalized %d\n", st);

  nss = gpio_pin_get_raw(csPort, csPin);
  dc  = gpio_pin_get_raw(dcPort, dcPin);
  res = gpio_pin_get_raw(resPort, resPin);
  printk("Ending GPIO pin states: NSS=%d DC=%d RES=%d\n", nss, dc, res);

  return 0;
}
