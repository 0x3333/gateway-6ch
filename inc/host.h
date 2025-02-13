#ifndef HOST_H_
#define HOST_H_

#include <FreeRTOS.h>
#include <queue.h>

#include "uart.h"
#include "bus.h"
#include "messages.h"

//
// Defines
//

#ifndef HOST_UART
#define HOST_UART hw_uart1
#endif
#ifndef HOST_UART_BUFFER_SIZE
// Don't know why I choose to use half the UART buffer size, but anyways.
#define HOST_UART_BUFFER_SIZE (UARTS_BUFFER_SIZE / 2)
#endif

#ifndef HOST_QUEUE_LENGTH
#define HOST_QUEUE_LENGTH 50
#endif

#ifndef HOST_HEARTBEAT_INTERVAL
#define HOST_HEARTBEAT_INTERVAL 1000
#endif

#ifndef HOST_MAX_HEARTBEAT_INTERVAL
#define HOST_MAX_HEARTBEAT_INTERVAL 5000
#endif

//
// Prototypes
//

extern QueueHandle_t host_change_queue;

void host_init(void);

#endif // HOST_H_