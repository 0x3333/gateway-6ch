#ifndef HOST_H_
#define HOST_H_

#include <FreeRTOS.h>
#include <queue.h>

#include "config.h"
#include "uart.h"
#include "bus.h"
#include "messages.h"

//
// Prototypes
//

extern QueueHandle_t host_change_queue;
extern QueueHandle_t host_command_queue;

void host_init(void);

#endif // HOST_H_