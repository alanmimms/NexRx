/*
 * Copyright (c) 2026 Starview Labs (starviewlabs.com)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT zephyr_uc1609

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/cache.h>
#include <zephyr/linker/section_tags.h>

LOG_MODULE_REGISTER(uc1609, CONFIG_UC1609_LOG_LEVEL);

#if defined(CONFIG_UC1609_LOG_LEVEL_DBG) || false
#define UC_DBG(fmt, ...) printk("[uc1609_dbg] " fmt "\n", ##__VA_ARGS__)
#define DBGRAW(fmt, ...) printk(fmt, ##__VA_ARGS__)
#else
#define UC_DBG(fmt, ...) do {} while (0)
#define DBGRAW(fmt, ...) do {} while (0)
#endif


/* UC1609 Command Set */
#define UC1609_CMD_SET_COL_ADDR_LSB     0x00
#define UC1609_CMD_SET_COL_ADDR_MSB     0x10
#define UC1609_CMD_SET_SCROLL_LINE      0x40
#define UC1609_CMD_SET_PAGE_ADDR        0xB0

#define UC1609_CMD_SET_POWER_CONTROL    0x28
#define UC1609_CMD_SET_VBIAS_POT        0x81
#define UC1609_CMD_SET_BIAS_RATIO       0xEB

#define UC1609_CMD_SET_RAM_ADDR_CTRL    0x88
#define UC1609_CMD_SET_ALL_PIX_ON       0xA4
#define UC1609_CMD_SET_ALL_PIX_NORMAL	0xA5

#define UC1609_CMD_SET_DISP_OFF		0xAE
#define UC1609_CMD_SET_DISP_ON		0xAF

#define UC1609_CMD_SYSTEM_RESET         0xE2

#define UC1609_PAGES_PER_ROW            8

struct uc1609_config {
  struct spi_dt_spec spi;
  struct gpio_dt_spec dc;
  struct gpio_dt_spec reset;
  uint16_t width;
  uint16_t height;
};

struct uc1609_data {
  uint8_t contrast;
};


// Largest CMD transaction we do
#define CMD_NBYTES 1

#define NBYTES (192 * 64 / 8)

static struct k_sem dmaDoneSem = Z_SEM_INITIALIZER(dmaDoneSem, 1, 1);


static int uc1609_write_cmd(const struct device *dev, uint8_t cmd)
{
  const struct uc1609_config *config = dev->config;
  static uint8_t __nocache dmaCmdBuf[CMD_NBYTES] __aligned(32);
  static const struct spi_buf buf = {.buf = &dmaCmdBuf, .len = 1};
  static const struct spi_buf_set tx = {.buffers = &buf, .count = 1};

  k_sem_take(&dmaDoneSem, K_FOREVER);

  UC_DBG("CMD: 0x%02X", cmd);
  dmaCmdBuf[0] = cmd;

  /* D/C low for command */
  gpio_pin_set_dt(&config->dc, 0);
  int st = spi_write_dt(&config->spi, &tx);
  k_sem_give(&dmaDoneSem);
  if (st < 0) LOG_ERR("spi_write_dt failed: %d\n", st);
  return st;
}

static int uc1609_write_data(const struct device *dev, const uint8_t *data, size_t len)
{
  const struct uc1609_config *config = dev->config;
  struct spi_buf buf = {
    .buf = (void *)data,
    .len = len
  };
  struct spi_buf_set tx = {
    .buffers = &buf,
    .count = 1
  };

  printk("uc1609_write_data is being called data=%p len=%u!\n", data, (unsigned) len);
  DBGRAW("DATA [%zu bytes]:", (unsigned) len);

  /* Truncate hex dump to first 16 or 32 bytes to avoid stalling the UART */
  const size_t dumpLen = (len > 32) ? 32 : len;
  //  LOG_HEXDUMP_DBG(data, dumpLen, "Data head:");
  for (int k=0; k < dumpLen; ++k) DBGRAW(" %02X", data[k]);
  DBGRAW("\n");

  /* D/C high for display RAM data */
  gpio_pin_set_dt(&config->dc, 1);
  int st = spi_write_dt(&config->spi, &tx);
  if (st < 0) LOG_ERR("spi_write_dt failed: %d\n", st);
  return st;
}

static int uc1609_set_contrast(const struct device *dev, uint8_t contrast)
{
  int ret = uc1609_write_cmd(dev, UC1609_CMD_SET_VBIAS_POT);
  if (ret < 0) {
    return ret;
  }
  return uc1609_write_cmd(dev, contrast);
}

static int uc1609_blanking_on(const struct device *dev)
{
  int ret = uc1609_write_cmd(dev, UC1609_CMD_SET_DISP_OFF);
  //  ret = uc1609_write_cmd(dev, UC1609_CMD_SET_ALL_PIX_ON);
  return ret;
}

static int uc1609_blanking_off(const struct device *dev)
{
  int ret = uc1609_write_cmd(dev, UC1609_CMD_SET_DISP_ON);
  //  ret = uc1609_write_cmd(dev, UC1609_CMD_SET_ALL_PIX_NORMAL);
  return ret;
}


static void spiAsyncCB(const struct device *devP, int result, void *dataP) {
  ARG_UNUSED(devP);
  ARG_UNUSED(dataP);

  if (result < 0) printk("spiAsyncCB result=%d\n", result);
  k_sem_give(&dmaDoneSem);
}


static int uc1609_write(const struct device *dev, const uint16_t x, const uint16_t y,
                        const struct display_buffer_descriptor *desc, const void *buf)
{
  const struct uc1609_config *config = dev->config;

  // Check for fast, frequent (only?) case that full display update is
  // required. We assume the page address and column are always left
  // at 0 and the DC pin is already set to DATA when we're called.
  if (x == 0 &&
      y == 0 &&
      desc->width == config->width &&
      desc->height == config->height)
  {
    int st = 0;

    k_sem_take(&dmaDoneSem, K_FOREVER);
    gpio_pin_set_dt(&config->dc, 1); /* Sending DATA */

    // Start async DMA
    struct spi_buf txBuf = {.buf = (void *) buf, .len = NBYTES};
    struct spi_buf_set txSet = {.buffers = &txBuf, .count = 1};

    const struct spi_dt_spec *spiP = &config->spi;

    printk("uc1690_write %s buf=%p len=%u\n", spiP->bus->name, txBuf.buf, txBuf.len);

    st = spi_transceive_cb(spiP->bus, &spiP->config, &txSet, NULL, spiAsyncCB, NULL);
    printk("spi_transceive_cb returned %d\n", st);
    if (st < 0) k_sem_give(&dmaDoneSem);
    return st;
  } else {
    size_t bytes_per_page = desc->width;
    uint8_t start_page = y / UC1609_PAGES_PER_ROW;
    uint8_t total_pages = (desc->height + UC1609_PAGES_PER_ROW - 1) / UC1609_PAGES_PER_ROW;

    LOG_INF("WARNING: Non-optimal call to uc1609 driver write(): x=%d, y=%d, w=%d, h=%d",
	    x, y, desc->width, desc->height);

    if (desc->pitch != desc->width) {
      LOG_ERR("Unsupported pitch %u != width %u", desc->pitch, desc->width);
      return -EINVAL;
    }

    if ((x + desc->width > config->width) || (y + desc->height > config->height)) {
      LOG_ERR("Buffer boundary exceeds display limits");
      return -EINVAL;
    }

    int ret = uc1609_write_cmd(dev, UC1609_CMD_SET_PAGE_ADDR | (start_page & 0x0F));
    if (ret < 0) {
      return ret;
    }

    ret = uc1609_write_cmd(dev, UC1609_CMD_SET_COL_ADDR_LSB | (x & 0x0F));
    if (ret < 0) {
      return ret;
    }

    ret = uc1609_write_cmd(dev, UC1609_CMD_SET_COL_ADDR_MSB | ((x >> 4) & 0x0F));
    if (ret < 0) {
      return ret;
    }

    const uint8_t *pixel_data = buf;

    for (unsigned page = 0; page < total_pages; page++) {
      ret = uc1609_write_data(dev, pixel_data, bytes_per_page);
      if (ret < 0) {
	return ret;
      }

      pixel_data += bytes_per_page;
    }
  }

  return 0;
}

static void uc1609_get_capabilities(const struct device *dev, struct display_capabilities *caps)
{
  const struct uc1609_config *config = dev->config;

  memset(caps, 0, sizeof(struct display_capabilities));
  caps->x_resolution = config->width;
  caps->y_resolution = config->height;
  caps->supported_pixel_formats = PIXEL_FORMAT_MONO01 | PIXEL_FORMAT_MONO10;
  caps->current_pixel_format = PIXEL_FORMAT_MONO10;
  caps->current_orientation = DISPLAY_ORIENTATION_NORMAL;
  caps->screen_info = SCREEN_INFO_MONO_VTILED;
}

static int uc1609_set_pixel_format(const struct device *dev, const enum display_pixel_format pixel_format)
{
  if (pixel_format == PIXEL_FORMAT_MONO10 || pixel_format == PIXEL_FORMAT_MONO01) {
    return 0;
  }
  LOG_ERR("Unsupported pixel format %d", pixel_format);
  return -ENOTSUP;
}

static int uc1609_set_orientation(const struct device *dev, const enum display_orientation orientation)
{
  UC_DBG("Set display orientation %d\n", (int) orientation);
  if (orientation == DISPLAY_ORIENTATION_NORMAL) {
    return 0;
  }
  LOG_ERR("Orientation not supported");
  return -ENOTSUP;
}

static int uc1609_init(const struct device *dev)
{
  const struct uc1609_config *config = dev->config;
  struct uc1609_data *data = dev->data;
  int ret;

  DBGRAW("\n>>> [EARLY BOOT] uc1609_init running for %s <<<\n\n", dev->name);

  if (!spi_is_ready_dt(&config->spi)) {
    LOG_ERR("SPI device not ready");
    return -ENODEV;
  }

  if (!gpio_is_ready_dt(&config->dc)) {
    LOG_ERR("D/C GPIO device not ready");
    return -ENODEV;
  }

  ret = gpio_pin_configure_dt(&config->dc, GPIO_OUTPUT_INACTIVE);
  if (ret < 0) {
    return ret;
  }

  if (config->reset.port) {
    if (!gpio_is_ready_dt(&config->reset)) {
      LOG_ERR("Reset GPIO device not ready");
      return -ENODEV;
    }

    ret = gpio_pin_configure_dt(&config->reset, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
      return ret;
    }

    gpio_pin_set_dt(&config->reset, 1);
    k_msleep(10);
    gpio_pin_set_dt(&config->reset, 0);
    k_msleep(10);
  }

  /* Reset chip state */
  ret = uc1609_write_cmd(dev, UC1609_CMD_SYSTEM_RESET);
  if (ret < 0) {
    return ret;
  }
  k_msleep(10);

  /* Frame rate 76FPS */
  ret = uc1609_write_cmd(dev, 0xA0);
  if (ret < 0) {
    return ret;
  }
  k_msleep(10);

  /* Bias ratio: 1/8 bias (0xEB) or 1/9 bias (0xEA) */
  ret = uc1609_write_cmd(dev, UC1609_CMD_SET_BIAS_RATIO);
  if (ret < 0) {
    return ret;
  }

  /* RAM address control: auto-increment column, then page, with wrap-around */
  ret = uc1609_write_cmd(dev, UC1609_CMD_SET_RAM_ADDR_CTRL | 0x01);
  if (ret < 0) {
    return ret;
  }

  /* Power control: internal charge pump on */
  ret = uc1609_write_cmd(dev, UC1609_CMD_SET_POWER_CONTROL | 0x0F);
  k_msleep(100);	      // Wait for V_LCD charge pump to come up
  if (ret < 0) {
    return ret;
  }

  /* LCD mapping control */
  ret = uc1609_write_cmd(dev, 0xC4);
  if (ret < 0) {
    return ret;
  }

  data->contrast = CONFIG_UC1609_DEFAULT_CONTRAST;
  ret = uc1609_set_contrast(dev, data->contrast);
  if (ret < 0) {
    return ret;
  }

  /* Turn display on */
  ret = uc1609_blanking_off(dev);
  if (ret < 0) {
    return ret;
  }

#if 0
  /* Invert display */
  ret = uc1609_write_cmd(dev, 0xA7);
  if (ret < 0) {
    return ret;
  }
#endif
  
  return 0;
}

static const struct display_driver_api uc1609_api = {
  .blanking_on = uc1609_blanking_on,
  .blanking_off = uc1609_blanking_off,
  .write = uc1609_write,
  .read = NULL,
  .get_capabilities = uc1609_get_capabilities,
  .set_pixel_format = uc1609_set_pixel_format,
  .set_orientation = uc1609_set_orientation,
};

#define UC1609_INIT(inst)                                                      \
  static struct uc1609_data uc1609_data_##inst;                                \
                                                                               \
  static const struct uc1609_config uc1609_config_##inst = {                   \
    .spi = SPI_DT_SPEC_INST_GET(inst, SPI_OP_MODE_MASTER | SPI_WORD_SET(8)),   \
    .dc = GPIO_DT_SPEC_INST_GET(inst, data_cmd_gpios),                         \
    .reset = GPIO_DT_SPEC_INST_GET_OR(inst, reset_gpios, {0}),                 \
    .width = DT_INST_PROP(inst, width),                                        \
    .height = DT_INST_PROP(inst, height),                                      \
  };                                                                           \
                                                                               \
  DEVICE_DT_INST_DEFINE(inst,                                                  \
                        uc1609_init,                                           \
                        NULL,                                                  \
                        &uc1609_data_##inst,                                   \
                        &uc1609_config_##inst,                                 \
                        POST_KERNEL,                                           \
                        CONFIG_DISPLAY_INIT_PRIORITY,                          \
                        &uc1609_api);

DT_INST_FOREACH_STATUS_OKAY(UC1609_INIT)
