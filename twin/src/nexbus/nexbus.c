#include "nexbus.h"
#include <zephyr/kernel.h>
#include <stm32h7xx.h>

/* NexBus is connected to PC6 (Tune.txrx) on STM32H753 */
#define NEXBUS_PORT GPIOC
#define NEXBUS_PIN  6

/* CMSIS exposes the configured system core clock */
extern uint32_t SystemCoreClock;

/* Spin-delay using the DWT cycle counter for nanosecond precision */
static inline void delayCycles(uint32_t cycles) {
  uint32_t start = DWT->CYCCNT;
  while ((DWT->CYCCNT - start) < cycles) {
    /* spin */
  }
}

/* Transmit a single ratio-encoded symbol */
static void sendSymbol(uint8_t symbol, uint32_t cyclesPerUs) {
  uint32_t highCycles;
  uint32_t lowCycles;
  
  uint32_t q1 = cyclesPerUs / 4; /* 250ns base unit */

  switch (symbol & 0x03) {
    case 0: /* 25% High */
      highCycles = q1;
      lowCycles  = q1 * 3;
      break;
    case 1: /* 50% High */
      highCycles = q1 * 2;
      lowCycles  = q1 * 2;
      break;
    case 2: /* 75% High */
      highCycles = q1 * 3;
      lowCycles  = q1;
      break;
    case 3: /* 100% High (actually 1.0us High + 250ns Low spacer) */
    default:
      highCycles = q1 * 4;
      lowCycles  = q1;
      break;
  }

  /* Set High */
  NEXBUS_PORT->BSRR = (1UL << NEXBUS_PIN);
  delayCycles(highCycles);
  /* Set Low */
  NEXBUS_PORT->BSRR = (1UL << (NEXBUS_PIN + 16));
  delayCycles(lowCycles);
}

void initNexBus(void) {
  /* Enable GPIOC clock */
  RCC->AHB4ENR |= RCC_AHB4ENR_GPIOCEN;
  
  /* Configure PC6 as General Purpose Output */
  NEXBUS_PORT->MODER = (NEXBUS_PORT->MODER & ~(3UL << (NEXBUS_PIN * 2))) | (1UL << (NEXBUS_PIN * 2));
  
  /* Very High speed for clean edges */
  NEXBUS_PORT->OSPEEDR |= (3UL << (NEXBUS_PIN * 2));
  
  /* Output low initially */
  NEXBUS_PORT->BSRR = (1UL << (NEXBUS_PIN + 16));

  /* Ensure DWT cycle counter is enabled */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void nexbusSendFrame(const NexbusFrame* frame) {
  uint32_t cyclesPerUs = SystemCoreClock / 1000000;
  
  /* Disable interrupts for precise timing (~20us maximum) */
  unsigned int key = irq_lock();

  for (int i = 0; i < NEXBUS_TARGET_COUNT; i++) {
    uint8_t data = frame->targetGpo[i];
    /* Target expects lower 2 bits first, then upper 2 bits */
    sendSymbol(data & 0x03, cyclesPerUs);
    sendSymbol((data >> 2) & 0x03, cyclesPerUs);
  }

  /* Mode Nybble: Target expects upper 2 bits first, then lower 2 bits */
  sendSymbol((frame->modeNybble >> 2) & 0x03, cyclesPerUs);
  sendSymbol(frame->modeNybble & 0x03, cyclesPerUs);

  /* Send SYNC Pulse (> 1.5us High, followed by return to Low) */
  NEXBUS_PORT->BSRR = (1UL << NEXBUS_PIN);
  delayCycles(cyclesPerUs * 2); /* 2.0us */
  NEXBUS_PORT->BSRR = (1UL << (NEXBUS_PIN + 16));
  delayCycles(cyclesPerUs * 2); /* 2.0us idle before returning */

  irq_unlock(key);
}

void nexbusSendBreak(void) {
  /* Hold bus Low for 150us (> 100us) to trigger Super-Break */
  NEXBUS_PORT->BSRR = (1UL << (NEXBUS_PIN + 16));
  k_busy_wait(150);
}
