#include <stdio.h>
#include <stdarg.h>

#include <FreeRTOS.h>
#include <task.h>

#include "debug.h"
#include "target/min.h"
#include "host.h"

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

    hw_uart_write_bytes(&HOST_UART, &byte, 1);
}

uint16_t min_tx_space(uint8_t port)
{
    (void)port;

    return UARTS_BUFFER_SIZE; // Technically not correct, but this call is silly anyways
}

uint32_t min_time_ms(void)
{
    return xTaskGetTickCount();
}
