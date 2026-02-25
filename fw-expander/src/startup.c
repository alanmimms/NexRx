#include <stdint.h>

extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;
extern uint32_t _estack;

extern void main(void);
void resetHandler(void);

void defaultHandler(void) {
  while (1) ;
}

/* ISR Vectors */
__attribute__((section(".isr_vector")))
const uint32_t isrVector[] = {
  (uint32_t)&_estack,
  (uint32_t)resetHandler,
  (uint32_t)defaultHandler, /* NMI */
  (uint32_t)defaultHandler, /* HardFault */
  0, 0, 0, 0, 0, 0, 0,       /* Reserved */
  (uint32_t)defaultHandler, /* SVC */
  0, 0,                      /* Reserved */
  (uint32_t)defaultHandler, /* PendSV */
  (uint32_t)defaultHandler, /* SysTick */
  /* External Interrupts */
  (uint32_t)defaultHandler, /* WWDG */
  0,                         /* Reserved */
  (uint32_t)defaultHandler, /* RTC */
  (uint32_t)defaultHandler, /* FLASH */
  (uint32_t)defaultHandler, /* RCC */
  (uint32_t)defaultHandler, /* EXTI0_1 */
  (uint32_t)defaultHandler, /* EXTI2_3 */
  (uint32_t)defaultHandler, /* EXTI4_15 */
  0,                         /* Reserved */
  (uint32_t)defaultHandler, /* DMA1_Channel1 */
  (uint32_t)defaultHandler, /* DMA1_Channel2_3 */
  (uint32_t)defaultHandler, /* DMAMUX */
  (uint32_t)defaultHandler, /* ADC1 */
  (uint32_t)defaultHandler, /* TIM1_BRK_UP_TRG_COM */
  (uint32_t)defaultHandler, /* TIM1_CC */
  (uint32_t)defaultHandler, /* TIM3 */
  0,                         /* Reserved */
  0,                         /* Reserved */
  (uint32_t)defaultHandler, /* TIM14 */
  0,                         /* Reserved */
  (uint32_t)defaultHandler, /* TIM16 */
  (uint32_t)defaultHandler, /* TIM17 */
  (uint32_t)defaultHandler, /* I2C1 */
  0,                         /* Reserved */
  (uint32_t)defaultHandler, /* SPI1 */
  0,                         /* Reserved */
  (uint32_t)defaultHandler, /* USART1 */
  0,                         /* Reserved */
  (uint32_t)defaultHandler, /* USART2 */
};

void resetHandler(void) {
  uint32_t* src;
  uint32_t* dest;

  /* Copy data section from FLASH to RAM */
  src = &_sidata;
  dest = &_sdata;
  while (dest < &_edata) {
    *dest++ = *src++;
  }

  /* Initialize BSS section to zero */
  dest = &_sbss;
  while (dest < &_ebss) {
    *dest++ = 0;
  }

  /* Call the application's entry point */
  main();

  /* Should not reach here */
  while (1) ;
}
