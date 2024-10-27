#include "modbus.h"

#include <string.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include "uart.h"
#include "protocol.h"

#ifndef MB_SEND_WAIT_TIME
#define MB_SEND_WAIT_TIME 30
#endif

QueueHandle_t mb_bus_queue;

// Manages a Bus communication
void task_bus_manager(void *arg)
{
    uint8_t bus = *((uint8_t *)arg);

    while (true)
    {
        taskYIELD();
    }
}

enum mb_return
{
    MB_ERROR = -1,
    MB_TIMEOUT = -2,
};

int32_t mb_send(uint8_t bus, uint8_t *payload, uint8_t payload_length)
{
    struct pio_uart *const uart = pio_uarts[bus];

    // Copy payload to DMA buffer
    memcpy(uart->tx_dma_buffer, payload, payload_length);
    pio_tx_dma_start_transfer(uart, payload_length);

    // FIXME: Wait for transmittion to finish

    uint8_t c;
    size_t received_length = xStreamBufferReceive(uart->rx_sbuffer, &c, 1, pdMS_TO_TICKS(MB_SEND_WAIT_TIME));

    return 0;
}