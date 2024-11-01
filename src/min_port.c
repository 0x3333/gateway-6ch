#include <stdio.h>
#include <stdarg.h>
#include <FreeRTOS.h>
#include <task.h>

#include "debug.h"
#include "target/min.h"
#include "uart.h"

uint8_t tx_buffer[UART_TX_BUFFER_SIZE];
uint8_t tx_buffer_index;

void min_tx_start(uint8_t port)
{
    (void)port;

    tx_buffer_index = 0;
}

void min_tx_finished(uint8_t port)
{
    (void)port;

    hw_uart_write(&esp_uart, (uint8_t *)&tx_buffer, tx_buffer_index);
}

void min_tx_byte(uint8_t port, uint8_t byte)
{
    (void)port;

    tx_buffer[tx_buffer_index++] = byte;
}

uint16_t min_tx_space(uint8_t port)
{
    (void)port;

    return TOTAL_BUFFER_SIZE; // Technically not correct, but works
}

uint32_t min_time_ms(void)
{
    return xTaskGetTickCount();
}

void min_send_ack(struct min_context *self, uint8_t min_id)
{
    const uint8_t payload[] = {0xFF};
    min_send_frame(self, min_id & (uint8_t)0x1FU, (const uint8_t *)&payload, sizeof payload * sizeof(uint8_t));
}

void min_application_handler(uint8_t min_id, uint8_t const *min_payload, uint8_t len_payload, uint8_t port)
{
    printf("ID: %u Port: %u - ", min_id, port);
    debug_array(min_payload, len_payload);

    // min_send_ack(self, min_id);
}
