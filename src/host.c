#include "host.h"

#include <stdio.h>
#include <FreeRTOS.h>
#include <task.h>
#include <target/min.h>

#include "uart.h"
#include "messages.h"
#include "bus.h"
#include "debug.h"

static struct min_context min_ctx;

void config_bus(struct m_config_bus *msg)
{
    struct pio_uart *uart;
    if (msg->bus == 1)
        uart = &pio_uart_1;
    else if (msg->bus == 2)
        uart = &pio_uart_2;
    else if (msg->bus == 3)
        uart = &pio_uart_3;
    else if (msg->bus == 4)
        uart = &pio_uart_4;
    else if (msg->bus == 5)
        uart = &pio_uart_5;
    else if (msg->bus == 6)
        uart = &pio_uart_6;
    else
    {
        printf("Invalid Bus number %u!", msg->bus);
        return;
    }

    if (msg->baudrate > 0)
    {
        uart->super.baudrate = msg->baudrate;
    }

    init_pio_uart(uart);
    init_bus(uart);

    // TODO: Check what else needs to be done here, maybe spawn a task to manage this bus messages
}

void config_mb_read(struct m_config_mb_read *msg)
{
}

void mb_read(struct m_mb_read *msg)
{
}

void mb_write(struct m_mb_write *msg)
{
}

// Handle Min Packet received
void min_application_handler(uint8_t min_id, uint8_t const *min_payload, uint8_t len_payload, uint8_t port)
{
    (void)port;

    printf("ID: %u - Len: %zu: ", min_id, len_payload);
    debug_array(min_payload, len_payload);

    switch (min_id)
    {
    case MESSAGE_CONFIG_MB_BUS:
        if (len_payload == sizeof(struct m_config_bus))
        {
            printf("MESSAGE_CONFIG_MB_BUS invalid size");
            return;
        }
        config_bus((struct m_config_bus *)min_payload);
        break;

    case MESSAGE_CONFIG_MB_READ:
        if (len_payload == sizeof(struct m_config_mb_read))
        {
            printf("MESSAGE_CONFIG_MB_READ invalid size");
            return;
        }
        config_mb_read((struct m_config_mb_read *)min_payload);
        break;

    case MESSAGE_MB_READ:
        if (len_payload == sizeof(struct m_mb_read))
        {
            printf("MESSAGE_MB_READ invalid size");
            return;
        }
        mb_read((struct m_mb_read *)min_payload);

        break;

    case MESSAGE_MB_WRITE:
        if (len_payload == sizeof(struct m_mb_write))
        {
            printf("MESSAGE_MB_WRITE invalid size");
            return;
        }
        mb_write((struct m_mb_write *)min_payload);

        break;
    }
}

static void task_host_handler(void *arg)
{
    (void)arg;

    uint8_t buffer[UARTS_BUFFER_SIZE] = {0};

    while (true)
    {
        uint8_t count = hw_uart_read_bytes(&HOST_UART, &buffer, UARTS_BUFFER_SIZE / 2);

        min_poll(&min_ctx, buffer, count);

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void init_host(void)
{
    init_hw_uart(&HOST_UART);

    min_init_context(&min_ctx, 0);

    xTaskCreate(task_host_handler, "Host Handler", configMINIMAL_STACK_SIZE, NULL, tskDEFAULT_PRIORITY, NULL);
}
