#ifndef DMX_H_
#define DMX_H_

#include "hardware/pio.h"
#include "hardware/dma.h"
#include <FreeRTOS.h>
#include <queue.h>

#include "config.h"

//
// Data Structures
//

//
// Variables
//

extern QueueHandle_t dmx_write_queue;

//
// Prototypes
//

void dmx_init();
void dmx_write(const uint8_t *universe, uint8_t length);
bool dmx_is_writable();

#endif