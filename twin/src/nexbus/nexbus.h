#ifndef NEXBUS_H
#define NEXBUS_H

#include <stdint.h>
#include <stdbool.h>

#define NEXBUS_TARGET_COUNT 4

typedef struct {
  uint8_t targetGpo[NEXBUS_TARGET_COUNT];
  uint8_t modeNybble;
} NexbusFrame;

/* Initialize the NexBus driver (configures PC6 and enables DWT cycle counter) */
void initNexBus(void);

/* Transmit a complete frame synchronously. Interrupts are disabled internally for ~20us */
void nexbusSendFrame(const NexbusFrame* frame);

/* Issue a Super-Break (logic Low for > 100us) to reset targets to Normal Mode */
void nexbusSendBreak(void);

#endif /* NEXBUS_H */
