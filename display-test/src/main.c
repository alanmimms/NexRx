#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/display/cfb.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/sys/printk.h>

int main(void) {
  const struct device *display = DEVICE_DT_GET(DT_NODELABEL(ssd1306));

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

  cfb_framebuffer_init(display);
  printk("Framebuffer initialized\n");
  cfb_framebuffer_clear(display, true);
  printk("Framebuffer cleared\n");

  // Draw text or push raw bitmap data to the screen
  cfb_print(display, "NexRx OLED Test", 0, 0);
  printk("Framebuffer string printed\n");
  cfb_framebuffer_finalize(display);
  printk("Framebuffer finalized\n");

  return 0;
}
