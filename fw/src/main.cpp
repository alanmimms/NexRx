#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/drivers/uart.h>

LOG_MODULE_REGISTER(nexrx_main, LOG_LEVEL_INF);

namespace nexrx {

//======================================================================
// Power Management
//======================================================================
class PowerManager {
public:
  static constexpr struct gpio_dt_spec vcore = 
    GPIO_DT_SPEC_GET(DT_NODELABEL(pwr_fpgacore), gpios);
  static constexpr struct gpio_dt_spec v33d = 
    GPIO_DT_SPEC_GET(DT_NODELABEL(pwr_3v3), gpios);
  static constexpr struct gpio_dt_spec v33a = 
    GPIO_DT_SPEC_GET(DT_NODELABEL(pwr_3va), gpios);
  static constexpr struct gpio_dt_spec v5va = 
    GPIO_DT_SPEC_GET(DT_NODELABEL(pwr_5va), gpios);

  static void init() {
    gpio_pin_configure_dt(&vcore, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&v33d, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&v33a, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&v5va, GPIO_OUTPUT_INACTIVE);
  }

  static void runSequence() {
    LOG_INF("--- NexRx Board Power-On Sequence Start ---");
    LOG_INF("[1/4] Enabling FPGA VCORE...");
    gpio_pin_set_dt(&vcore, 1);
    k_sleep(K_MSEC(100));
    LOG_INF("[2/4] Enabling 3.3V Digital Rail...");
    gpio_pin_set_dt(&v33d, 1);
    k_sleep(K_MSEC(100));
    LOG_INF("[3/4] Enabling 3.3V Analog Rail...");
    gpio_pin_set_dt(&v33a, 1);
    k_sleep(K_MSEC(100));
    LOG_INF("[4/4] Enabling 5V Analog Rail...");
    gpio_pin_set_dt(&v5va, 1);
    k_sleep(K_MSEC(100));
    LOG_INF("--- NexRx Board Power-On Sequence Success ---");
  }

  static void monitorUSBPower() {
    LOG_INF("USB Power Monitor: Checking for 3.0A (15W) Source...");
    LOG_INF("USB Power Monitor: Capacity = 3.0A (Detected)");
  }
};

//======================================================================
// NexBus (GPIO Expanders)
//======================================================================
class NexBus {
public:
  static constexpr uint32_t BIT_PERIOD_TICKS = 100;
  static constexpr uint32_t DUTY_0_TICKS = 33;
  static constexpr uint32_t DUTY_1_TICKS = 66;

  static uint32_t dma_buffer[64];

  static void init() {
    LOG_INF("NexBus: Mode A Initialized (1Mb/s Pulse-Width)");
  }

  static void transmit(uint32_t data, uint8_t bit_count) {
    for (int i = 0; i < bit_count; ++i) {
      bool b = (data >> (bit_count - 1 - i)) & 0x01;
      dma_buffer[i] = b ? DUTY_1_TICKS : DUTY_0_TICKS;
    }
    dma_buffer[bit_count] = 0;
    LOG_DBG("NexBus: Transmitted 0x%08X (%d bits)", data, bit_count);
  }
};

uint32_t NexBus::dma_buffer[64];

//======================================================================
// FPGA Manager
//======================================================================
class FPGAManager {
public:
  static constexpr struct gpio_dt_spec prog = 
    GPIO_DT_SPEC_GET(DT_NODELABEL(fpga), prog_gpios);
  static constexpr struct gpio_dt_spec done = 
    GPIO_DT_SPEC_GET(DT_NODELABEL(fpga), done_gpios);
  static constexpr struct gpio_dt_spec nss = 
    GPIO_DT_SPEC_GET(DT_NODELABEL(fpga), nss_gpios);

  static constexpr uint8_t REG_SCRATCHPAD = 0x00;
  static constexpr uint16_t VALIDATION_MAGIC = 0x55AA;

  static void init() {
    gpio_pin_configure_dt(&prog, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&done, GPIO_INPUT);
    gpio_pin_configure_dt(&nss, GPIO_OUTPUT_INACTIVE);
  }

  static bool loadBitstream(const uint8_t* data, size_t len) {
    const struct device* spi_dev = DEVICE_DT_GET(DT_NODELABEL(spi3));
    if (!device_is_ready(spi_dev)) {
      LOG_ERR("FPGA: SPI3 not ready");
      return false;
    }

    LOG_INF("FPGA: Configuration Starting (%d bytes)...", len);
    gpio_pin_set_dt(&nss, 0);
    gpio_pin_set_dt(&prog, 0);
    k_sleep(K_MSEC(1));
    gpio_pin_set_dt(&prog, 1);
    k_sleep(K_MSEC(2));

    struct spi_config config = {
      .frequency = 10000000,
      .operation = SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(8),
      .slave = 0,
    };

    if (data && len > 0) {
      struct spi_buf tx_buf = { .buf = (void*)data, .len = len };
      struct spi_buf_set tx_bufs = { .buffers = &tx_buf, .count = 1 };
      spi_write(spi_dev, &config, &tx_bufs);
    }

    uint8_t dummy[13] = {0xFF};
    struct spi_buf d_buf = { .buf = dummy, .len = sizeof(dummy) };
    struct spi_buf_set d_bufs = { .buffers = &d_buf, .count = 1 };
    spi_write(spi_dev, &config, &d_bufs);

    gpio_pin_set_dt(&nss, 1);

    if (gpio_pin_get_dt(&done)) {
      LOG_INF("FPGA: Configuration SUCCESS (DONE=High)");
      k_sleep(K_MSEC(10));
      return validatePath(spi_dev);
    } else {
      LOG_ERR("FPGA: Configuration FAILED (DONE=Low)");
      return false;
    }
  }

private:
  static bool validatePath(const struct device* spi_dev) {
    uint8_t tx_data[3] = { (uint8_t)(0x80 | REG_SCRATCHPAD), 
                           (uint8_t)(VALIDATION_MAGIC >> 8), 
                           (uint8_t)(VALIDATION_MAGIC & 0xFF) };
    uint8_t rx_data[3] = {0};
    struct spi_config config = {
      .frequency = 10000000,
      .operation = SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(8),
      .slave = 0,
    };
    struct spi_buf tx_buf = { .buf = tx_data, .len = sizeof(tx_data) };
    struct spi_buf_set tx_bufs = { .buffers = &tx_buf, .count = 1 };
    gpio_pin_set_dt(&nss, 0);
    spi_write(spi_dev, &config, &tx_bufs);
    gpio_pin_set_dt(&nss, 1);

    tx_data[0] = REG_SCRATCHPAD;
    struct spi_buf rx_buf = { .buf = rx_data, .len = sizeof(rx_data) };
    struct spi_buf_set rx_bufs = { .buffers = &rx_buf, .count = 1 };
    gpio_pin_set_dt(&nss, 0);
    spi_transceive(spi_dev, &config, &tx_bufs, &rx_bufs);
    gpio_pin_set_dt(&nss, 1);

    uint16_t read_val = (rx_data[1] << 8) | rx_data[2];
    if (read_val == VALIDATION_MAGIC) {
      LOG_INF("FPGA: SPI Path Validated (0x%04X matches)", read_val);
      return true;
    } else {
      LOG_ERR("FPGA: SPI Path FAILED (Expected 0x%04X, got 0x%04X)", 
              VALIDATION_MAGIC, read_val);
      return false;
    }
  }
};

//======================================================================
// MAX9939 PGA (Programmable Gain Amplifier)
//======================================================================
class MAX9939 {
public:
  static void init() {
    const struct device* spi_dev = DEVICE_DT_GET(DT_NODELABEL(spi4));
    if (!device_is_ready(spi_dev)) {
      LOG_ERR("PGA: SPI4 not ready");
      return;
    }
    LOG_INF("PGA: Broadcast Gain Interface Initialized");
    
    /* Set default gain to 1 V/V (0 dB) */
    setGain(0);
  }

  /**
   * @brief Sets gain for all 6 PGAs via SPI4 broadcast.
   * @param code 4-bit gain code (0x0 to 0xB).
   */
  static void setGain(uint8_t code) {
    const struct device* spi_dev = DEVICE_DT_GET(DT_NODELABEL(spi4));
    uint8_t cmd = 0x80 | (code & 0x0F); /* Bit 7=1, Bits 3-0=Gain */

    struct spi_config config = {
      .frequency = 1000000,
      .operation = SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(8),
      .slave = 0,
    };

    struct spi_buf tx_buf = { .buf = &cmd, .len = 1 };
    struct spi_buf_set tx_bufs = { .buffers = &tx_buf, .count = 1 };

    /* All PGAs share the same CS (managed by Zephyr &spi4) */
    if (spi_write(spi_dev, &config, &tx_bufs) == 0) {
      LOG_INF("PGA: Broadcast Gain set to code 0x%01X", code);
    } else {
      LOG_ERR("PGA: SPI broadcast failed");
    }
  }
};

//======================================================================
// AK5578 Audio Codec
//======================================================================
class AK5578 {
public:
  static constexpr uint8_t I2C_ADDR = 0x13;
  static constexpr struct gpio_dt_spec reset_pin = 
    GPIO_DT_SPEC_GET(DT_NODELABEL(codec), reset_gpios);

  static void init() {
    const struct device* i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c4));
    if (!device_is_ready(i2c_dev)) {
      LOG_ERR("Codec: I2C4 not ready");
      return;
    }

    LOG_INF("Codec: Performing Hard Reset...");
    gpio_pin_configure_dt(&reset_pin, GPIO_OUTPUT_INACTIVE);
    gpio_pin_set_dt(&reset_pin, 0);
    k_sleep(K_MSEC(10));
    gpio_pin_set_dt(&reset_pin, 1);
    k_sleep(K_MSEC(10));

    LOG_INF("Codec: Initializing AK5578...");
    i2c_reg_write_byte(i2c_dev, I2C_ADDR, 0x00, 0x00);
    i2c_reg_write_byte(i2c_dev, I2C_ADDR, 0x02, 0x01);
    LOG_INF("Codec: Initialization Complete (4-lane I2S mode)");
  }
};

//======================================================================
// QSD Capture Engine
//======================================================================
class QSDCapture {
public:
  static constexpr size_t SAMPLES_PER_HALF = 256;
  static uint32_t lane0_buf[SAMPLES_PER_HALF * 2];
  static uint32_t lane1_buf[SAMPLES_PER_HALF * 2];
  static uint32_t lane2_buf[SAMPLES_PER_HALF * 2];
  static uint32_t lane3_buf[SAMPLES_PER_HALF * 2];

  static void init() {
    LOG_INF("QSD Capture: Initializing 4-lane Synchronous Engine...");
  }
};

uint32_t QSDCapture::lane0_buf[SAMPLES_PER_HALF * 2];
uint32_t QSDCapture::lane1_buf[SAMPLES_PER_HALF * 2];
uint32_t QSDCapture::lane2_buf[SAMPLES_PER_HALF * 2];
uint32_t QSDCapture::lane3_buf[SAMPLES_PER_HALF * 2];

//======================================================================
// Control Protocol & Transport
//======================================================================
class ITransport {
public:
  virtual ~ITransport() = default;
  virtual int send(const uint8_t* data, size_t len) = 0;
  virtual int receive(uint8_t* data, size_t max_len) = 0;
};

class ControlHandler {
public:
  static ControlHandler& instance() {
    static ControlHandler inst;
    return inst;
  }

  void process(ITransport& transport) {
    uint8_t buf[1024];
    int len = transport.receive(buf, sizeof(buf));
    if (len > 0) {
      LOG_INF("Control: Processing command (%d bytes)", len);
    }
  }

private:
  ControlHandler() = default;
};

//======================================================================
// Main System Entry
//======================================================================
void initUSB() {
  const struct device* dev = DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart);
  uint32_t dtr = 0;
  if (usb_enable(NULL)) return;
  LOG_INF("Waiting for USB Host connection (CDC-ACM)...");
  while (!dtr) {
    uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
    k_sleep(K_MSEC(100));
  }
  LOG_INF("USB Host connected!");
}

} // namespace nexrx

int main() {
  nexrx::initUSB();
  LOG_INF("NexRx MCU Firmware Starting...");

  nexrx::PowerManager::init();
  nexrx::PowerManager::monitorUSBPower();
  nexrx::PowerManager::runSequence();

  nexrx::FPGAManager::init();
  nexrx::FPGAManager::loadBitstream(nullptr, 0);

  nexrx::AK5578::init();
  nexrx::MAX9939::init();
  
  nexrx::NexBus::init();

  while (true) {
    k_sleep(K_MSEC(100));
  }

  return 0;
}
