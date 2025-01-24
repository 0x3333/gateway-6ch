#include "host.h"

#include <stdio.h>
#include <inttypes.h>
#include <FreeRTOS.h>
#include <task.h>
#include <target/min.h>

#include "uart.h"
#include "messages.h"
#include "bus.h"
#include "debug.h"

static struct min_context min_ctx;

//
// Message Handlers
//

void handle_m_config_bus(struct m_config_bus *msg)
{
    printf("Configuring bus %" PRIu8 "...\n", msg->bus);
    struct bus_context *bus_context = pvPortMalloc(sizeof(struct bus_context) + (msg->periodic_reads_size * sizeof(struct periodic_reads)));

    struct pio_uart *pio_uart = get_pio_uart_by_index(msg->bus);
    if (pio_uart == NULL || pio_uart->super.id != msg->bus)
    {
        vPortFree(bus_context);
        printf("Invalid Bus number %" PRIu8 "!", msg->bus);
        return;
    }

    bus_context->pio_uart = pio_uart;
    bus_context->baudrate = msg->baudrate;
    bus_context->bus = msg->bus;
    bus_context->periodic_interval = msg->periodic_interval;
    bus_context->periodic_reads_size = msg->periodic_reads_size;
    for (size_t i = 0; i < bus_context->periodic_reads_size; i++)
    {
        printf("Periodic read 0x%" PRIx8 " 0x%" PRIx8 " 0x%" PRIx8 "0x%" PRIx8 "\n ",
               msg->periodic_reads[i].slave,
               msg->periodic_reads[i].function,
               msg->periodic_reads[i].address,
               msg->periodic_reads[i].length);
        bus_context->periodic_reads[i].slave = msg->periodic_reads[i].slave;
        bus_context->periodic_reads[i].function = msg->periodic_reads[i].function;
        bus_context->periodic_reads[i].address = msg->periodic_reads[i].address;
        bus_context->periodic_reads[i].length = msg->periodic_reads[i].length;
        bus_context->periodic_reads[i].last_run = 0;
    }

    bus_init(bus_context);
}

void handle_m_read(struct m_read *msg)
{
    (void)msg;
}

void handle_m_write(struct m_write *msg)
{
    (void)msg;
}

// Handle Min Packet received
void min_application_handler(uint8_t min_id, uint8_t const *min_payload, uint8_t len_payload, uint8_t port)
{
    (void)port;

    debug(min_id, len_payload);
    debug_array(min_payload, len_payload);

    // Find the handler for this min_id message
    for (size_t i = 0; i < sizeof(m_handlers) / sizeof(struct m_handler); i++)
    {
        if (m_handlers[i].message_id == min_id)
        {
            m_handlers[i].handler(min_payload);
            return;
        }
    }
}

static void task_host_handler(void *arg)
{
    (void)arg;

    uint8_t buffer[HOST_UART_BUFFER_SIZE] = {0};

    while (true)
    {
        uint8_t count = hw_uart_read_bytes(&HOST_UART, &buffer, HOST_UART_BUFFER_SIZE);

        min_poll(&min_ctx, buffer, count);

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void host_init(void)
{
    hw_uart_init(&HOST_UART);

    min_init_context(&min_ctx, 0);

    xTaskCreate(task_host_handler, "Host Handler", configMINIMAL_STACK_SIZE, NULL, tskDEFAULT_PRIORITY, NULL);
}
