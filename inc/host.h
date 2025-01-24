#ifndef HOST_H_
#define HOST_H_

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

//
// Prototypes
//

void host_init(void);

#endif // HOST_H_