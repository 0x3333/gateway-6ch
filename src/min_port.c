#include <stdio.h>
#include <stdarg.h>

#include <FreeRTOS.h>
#include <task.h>

#include "macrologger.h"

#include "target/min.h"

#include "uart.h"
#include "config.h"

void min_tx_start(uint8_t port)
{
    (void)port;
}

void min_tx_finished(uint8_t port)
{
    (void)port;
}

void min_tx_byte(uint8_t port, uint8_t byte)
{
    (void)port;
    hw_uart_write_bytes_blocking(&HOST_UART, &byte, 1);
}

uint16_t min_tx_space(uint8_t port)
{
    (void)port;
    return hw_uart_tx_buffer_remaining(&HOST_UART);
}

uint32_t min_time_ms(void)
{
    return xTaskGetTickCount();
}

#ifdef MIN_DEBUG_PRINTING
void min_debug_print(const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    vprintf(msg, args);
    va_end(args);
}
#endif