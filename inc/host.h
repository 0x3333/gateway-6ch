#ifndef HOST_H_
#define HOST_H_

#include <FreeRTOS.h>
#include <queue.h>

#include "config.h"

//
// Variables
//

extern QueueHandle_t host_change_queue;
extern QueueHandle_t host_command_queue;

//
// Prototypes
//

void host_init(void);

#endif // HOST_H_