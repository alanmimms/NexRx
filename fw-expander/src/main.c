#include "stm32c0xx.h"
#include <stdint.h>

/* NexBus Protocol Constants */
#define CLOCK_FREQ_HZ       48000000UL
#define SYNC_MIN_US         1.5f
#define PULSE_MAX_US        5.0f
#define UNIT_US             0.25f

/* 
 * Calculate ticks at compile time. 
 * SYNC pulse must be > 1.5us (72 ticks @ 48MHz).
 * Symbol units are 250ns (12 ticks @ 48MHz).
 */
#define SYNC_MIN_TICKS      ((uint32_t)(CLOCK_FREQ_HZ * (SYNC_MIN_US / 1000000.0f)))
#define PULSE_MAX_TICKS     ((uint32_t)(CLOCK_FREQ_HZ * (PULSE_MAX_US / 1000000.0f)))
#define UNIT_TICKS          ((uint32_t)(CLOCK_FREQ_HZ * (UNIT_US / 1000000.0f)))

#define TARGET_COUNT        4
#define BITS_PER_TARGET     4
#define MODE_BITS           4
#define FRAME_BITS          (TARGET_COUNT * BITS_PER_TARGET + MODE_BITS)

/* Symbol Thresholds (midpoints between 12, 24, 36, 48 ticks) */
#define S0_MAX_TICKS        (18)
#define S1_MAX_TICKS        (30)
#define S2_MAX_TICKS        (42)

/* Hardware Mapping */
#define BUS_PIN             11      /* PA11 */
#define IWDG_TIMEOUT_MS     50      /* Reset if silent for >50ms */
#define IDLE_BLINK_MS       500
#define IDLE_TIMEOUT_MS     1000

/* IWDG Magic Keys (Defined by STM32 Hardware) */
#define IWDG_KEY_RELOAD     0xAAAA  /* Writing 0xAAAA to KR reloads the watchdog counter from RLR */
#define IWDG_KEY_ENABLE     0xCCCC  /* Writing 0xCCCC starts the watchdog timer */
#define IWDG_KEY_ACCESS     0x5555  /* Writing 0x5555 disables write protection for PR and RLR registers */

static void systemClockConfig(void);
static void initGPIO(void);
static uint8_t detectId(void);
static void jumpToBootloader(void);
static void initIWDG(void);

volatile uint32_t msTicks = 0;
uint8_t myId = 0;
uint8_t currentGpo = 0;

void SysTick_Handler(void) {
  msTicks++;
}

int main(void) {
  systemClockConfig();
  
  /* Check if reset was caused by the IWDG */
  if (RCC->CSR & RCC_CSR_IWDGRSTF) {
    /* Clear reset flags and jump to bootloader for recovery/maintenance */
    RCC->CSR |= RCC_CSR_RMVF;
    jumpToBootloader();
  }
  
  initGPIO();
  
  myId = detectId();
  
  /* 1ms system heartbeat */
  SysTick_Config(CLOCK_FREQ_HZ / 1000);

  /* Initialize TIM1 for NexBus Pulse Width Measurement */
  RCC->APBENR2 |= RCC_APBENR2_TIM1EN;
  
  /* PA11 as AF4 (TIM1_CH4) */
  GPIOA->MODER = (GPIOA->MODER & ~(3U << (BUS_PIN * 2))) | (2U << (BUS_PIN * 2));
  GPIOA->AFR[1] = (GPIOA->AFR[1] & ~(0xFU << ((BUS_PIN - 8) * 4))) | (4U << ((BUS_PIN - 8) * 4));

  /* 
   * Configure TIM1_CH4 for Input Capture:
   * 1. Capture on Falling Edge (measure High pulse duration).
   * 2. Slave Mode Reset on Rising Edge (TS=111 for TI4FP4).
   * This allows the CPU to read CCR4 as the absolute width of the last pulse.
   */
  TIM1->CCMR2 = TIM_CCMR2_CC4S_0;
  TIM1->CCER = TIM_CCER_CC4P;
  TIM1->SMCR = (7U << TIM_SMCR_TS_Pos) | (4U << TIM_SMCR_SMS_Pos); 
  TIM1->CR1 |= TIM_CR1_CEN;

  initIWDG();

  int bitCount = -1;
  uint32_t frameData = 0;
  uint32_t lastSyncMs = 0;
  uint32_t lastBlinkMs = 0;

  /* Initialize GPOs to safe state (All 0, Complements 1) */
  GPIOA->BSRR = 0x00F0000F; 

  while (1) {
    /* 
     * High-speed Polling Path:
     * We have ~48 clock cycles between pulses to process the previous symbol.
     */
    if (TIM1->SR & TIM_SR_CC4IF) {
      uint32_t pw = TIM1->CCR4;
      TIM1->SR = ~TIM_SR_CC4IF; 

      if (pw > SYNC_MIN_TICKS && pw < PULSE_MAX_TICKS) {
        /* SYNC Frame Delimiter */
        if (bitCount == FRAME_BITS) {
          uint8_t mode = (uint8_t)(frameData & 0x0F);
          if (mode == myId) {
            jumpToBootloader();
          }
        }
        bitCount = 0;
        frameData = 0;
        lastSyncMs = msTicks;
        IWDG->KR = IWDG_KEY_RELOAD; /* Reset watchdog on every valid SYNC */
      } else if (bitCount >= 0 && pw < PULSE_MAX_TICKS) {
        /* Data Symbol Decoding */
        uint8_t symbol;
        if (pw < S0_MAX_TICKS) symbol = 0;      
        else if (pw < S1_MAX_TICKS) symbol = 1; 
        else if (pw < S2_MAX_TICKS) symbol = 2; 
        else symbol = 3;              

        frameData = (frameData << 2) | symbol;
        
        /* Check if these 2 bits belong to this target's address */
        int myBitPos = myId * BITS_PER_TARGET;
        if (bitCount == myBitPos) {
          currentGpo = (currentGpo & 0x0C) | symbol;
        } else if (bitCount == myBitPos + 2) {
          currentGpo = (currentGpo & 0x03) | (symbol << 2);
          
          /* 
           * Update GPOs with staggered timing to reduce di/dt switching noise.
           * 1. Atomic update of True GPOs (PA0-PA3).
           * 2. Delay 21-42ns (hardware NOPs).
           * 3. Atomic update of Complement GPOs (PA4-PA7).
           */
          GPIOA->BSRR = (currentGpo & 0x0F) | ((~currentGpo & 0x0F) << 16);
          __NOP(); __NOP();
          GPIOA->BSRR = ((~currentGpo & 0x0F) << 4) | ((currentGpo & 0x0F) << 20);
        }
        bitCount += 2;
      }
    }

    /* 
     * Low-speed Diagnostic Path:
     * Only runs when the bus is idle to avoid impacting high-speed decoding.
     */
    if (bitCount < 0 && (msTicks - lastSyncMs) > IDLE_TIMEOUT_MS) {
      if ((msTicks - lastBlinkMs) > IDLE_BLINK_MS) {
        GPIOA->ODR ^= GPIO_ODR_OD0;
        lastBlinkMs = msTicks;
      }
      /* IWDG reload intentionally removed to allow silence-to-reset */
    }
  }
}

static void systemClockConfig(void) {
  FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY) | FLASH_ACR_LATENCY_1;
  RCC->CR |= RCC_CR_HSION;
  while (!(RCC->CR & RCC_CR_HSIRDY)) ;
  RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_0;
  while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_0) ;
}

static void initGPIO(void) {
  RCC->IOPENR |= RCC_IOPENR_GPIOAEN | RCC_IOPENR_GPIOBEN | RCC_IOPENR_GPIOCEN;
  /* Configure PA0-PA7 as Low Speed Outputs to minimize RF interference */
  GPIOA->MODER = (GPIOA->MODER & ~0x0000FFFF) | 0x00005555;
  GPIOA->OSPEEDR &= ~0x0000FFFF;
}

/**
 * detectId strategy:
 * Probes the hardware identity pins (PB7, PC14, PC15) to determine a trinary address.
 * Each pin is sampled twice to distinguish between High, Low, and Floating:
 * 1. Sample with internal Pull-Down enabled: If pin is High, it is tied to VCC (State 2).
 * 2. Sample with internal Pull-Up enabled: If pin is Low, it is tied to GND (State 0).
 * 3. If neither (Low with PD, High with PU), the pin is Floating (State 1).
 * The results are combined into a base-3 integer providing 27 unique IDs.
 */
static uint8_t detectId(void) {
  uint8_t states[3] = {0, 0, 0};
  GPIO_TypeDef* ports[] = {GPIOB, GPIOC, GPIOC};
  int pins[] = {7, 14, 15};

  for (int i = 0; i < 3; i++) {
    /* Test for High */
    ports[i]->PUPDR = (ports[i]->PUPDR & ~(3U << (pins[i] * 2))) | (2U << (pins[i] * 2));
    for (volatile int d = 0; d < 500; d++) ;
    uint32_t highPd = (ports[i]->IDR >> pins[i]) & 1;

    /* Test for Low */
    ports[i]->PUPDR = (ports[i]->PUPDR & ~(3U << (pins[i] * 2))) | (1U << (pins[i] * 2));
    for (volatile int d = 0; d < 500; d++) ;
    uint32_t lowPu = !((ports[i]->IDR >> pins[i]) & 1);

    if (highPd) states[i] = 2;      
    else if (lowPu) states[i] = 0;  
    else states[i] = 1;              
    ports[i]->PUPDR &= ~(3U << (pins[i] * 2)); 
  }
  return (uint8_t)(states[0] + states[1] * 3 + states[2] * 9);
}

static void initIWDG(void) {
  IWDG->KR = IWDG_KEY_ACCESS; /* Unlock PR and RLR registers */
  IWDG->PR = 0x03;            /* Prescaler /32 (LSI 32kHz / 32 = 1kHz clock) */
  IWDG->RLR = IWDG_TIMEOUT_MS;
  IWDG->KR = IWDG_KEY_ENABLE; /* Start the watchdog timer */
}

static void jumpToBootloader(void) {
  __disable_irq();
  TIM1->CR1 &= ~TIM_CR1_CEN;
  void (*sysMemBootloader)(void);
  /* System Memory start address for STM32C0 series */
  sysMemBootloader = (void (*)(void)) (*((uint32_t*) 0x1FFF0004));
  __set_MSP(*((uint32_t*) 0x1FFF0000));
  sysMemBootloader();
  while (1) ;
}
