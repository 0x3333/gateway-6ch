#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <target/min.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include "host.h"
#include "modbus.h"

#include "debug.h"

static struct min_context min_ctx;

QueueHandle_t host_change_queue;

//
// Message
//

void handle_m_config_bus(struct m_config_bus *msg)
{
    if (bus_get_context(msg->bus))
    {
        printf("Bus %" PRIu8 " already configured!\n", msg->bus);
        return;
    }
    printf("Configuring bus %" PRIu8 "...\n", msg->bus);

    struct pio_uart *pio_uart = get_pio_uart_by_index(msg->bus);
    if (pio_uart == NULL || pio_uart->super.id != msg->bus)
    {
        printf("Invalid Bus number %" PRIu8 "!", msg->bus);
        return;
    }

    // Calculate the size of all elements in the struct
    size_t bus_context_size = sizeof(struct bus_context) +
                              (sizeof(struct bus_periodic_read) * msg->periodic_reads_len);
    struct bus_context *bus_context = pvPortMalloc(bus_context_size);
    memset(bus_context, 0, bus_context_size);

    bus_context->pio_uart = pio_uart;
    bus_context->baudrate = msg->baudrate;
    bus_context->bus = msg->bus;
    bus_context->periodic_interval = msg->periodic_interval;
    bus_context->periodic_reads_len = msg->periodic_reads_len;
    for (size_t i = 0; i < bus_context->periodic_reads_len; i++)
    {
        struct bus_periodic_read *bus_pr = &bus_context->periodic_reads[i];
        struct m_base *msg_pr = &msg->periodic_reads[i];

        bus_pr->slave = msg_pr->slave;
        bus_pr->function = msg_pr->function;
        bus_pr->address = msg_pr->address;
        bus_pr->length = msg_pr->length;
        bus_pr->last_run = xTaskGetTickCount();
        bus_pr->memory_map_size = modbus_function_return_size(bus_pr->function, bus_pr->length);
        bus_pr->memory_map = pvPortMalloc(bus_pr->memory_map_size);
        memset(bus_pr->memory_map, 0, bus_pr->memory_map_size);
        printf("Bus PR read 0x%" PRIx8 " 0x%" PRIx8 " 0x%" PRIx16 " 0x%" PRIx16 "\n", bus_pr->slave, bus_pr->function, bus_pr->address, bus_pr->length);
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

    struct modbus_change change;
    uint8_t uart_read_buffer[HOST_UART_BUFFER_SIZE] = {0};

    for (;;) // Task infinite loop
    {
        uint8_t count = hw_uart_read_bytes(&HOST_UART, &uart_read_buffer, HOST_UART_BUFFER_SIZE);
        min_poll(&min_ctx, uart_read_buffer, count);

        if (xQueueReceive(host_change_queue, &change, 0) == pdTRUE)
        {
            printf("Change: Slave %" PRIu8 " Address %" PRIu16 " Value %" PRIu16 "\n",
                   change.slave_id, change.address, change.value);

            // TODO: Send the change to the host
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void host_init(void)
{
    hw_uart_init(&HOST_UART);

    min_init_context(&min_ctx, 0);

    host_change_queue = xQueueCreate(HOST_QUEUE_LENGTH, sizeof(struct modbus_change));

    xTaskCreate(task_host_handler, "Host Handler", configMINIMAL_STACK_SIZE, NULL, tskDEFAULT_PRIORITY, NULL);
}
