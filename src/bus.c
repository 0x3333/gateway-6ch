#include <FreeRTOS.h>
#include <task.h>

#include "bus.h"

static void bus_task(void *arg)
{
    struct pio_uart *pio_uart = arg;

    uint8_t buffer[UARTS_BUFFER_SIZE] = {0};

    while (true)
    {
        size_t read = pio_uart_read_bytes(pio_uart, &buffer, UARTS_BUFFER_SIZE);
        if (read > 0)
        {
        }
    }
}

void init_bus(struct pio_uart *pio_uart)
{
    char name[] = "Bus X";

    name[sizeof(name) / sizeof(char) - 2] = (char)(48 + pio_uart->super.id);

    xTaskCreate(bus_task, name, configMINIMAL_STACK_SIZE, pio_uart, tskDEFAULT_PRIORITY, NULL);

    // TODO: Check what needs to be done here
}