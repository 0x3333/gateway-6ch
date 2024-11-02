#include "modbus.h"

#include <string.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include "uart.h"
#include "messages.h"

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
        vTaskDelay(pdMS_TO_TICKS(1));
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

    pio_uart_write(uart, payload, payload_length);

    // FIXME: Wait for transmittion to finish

    uint8_t c;
    size_t received_length = pio_uart_read(uart, &c, 1);

    return 0;
}