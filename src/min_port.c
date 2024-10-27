#include <FreeRTOS.h>
#include <task.h>

#include "target/min.h"
#include "uart.h"

void min_tx_start(uint8_t port) {}

void min_tx_finished(uint8_t port) {}

void min_tx_byte(uint8_t port, uint8_t byte)
{
    uart_putc_raw(esp_uart.uart, byte);
}

uint16_t min_tx_space(uint8_t port)
{
    return 255; // FIXME: Validate
}

uint32_t min_time_ms(void)
{
    return xTaskGetTickCount();
}
