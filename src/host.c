#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <target/min.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include "hardware/watchdog.h"

#include "macrologger.h"

#include "host.h"
#include "modbus.h"

// Just to make it clear that 0 means no delay in StreamBuffer calls
static const uint8_t NO_DELAY = 0;

static struct min_context min_ctx;
static volatile TickType_t last_heartbeat_received;

QueueHandle_t host_change_queue;

//
// Message
//

void handle_m_config_bus(const struct m_config_bus *msg)
{
    if (bus_get_context(msg->bus))
    {
        LOG_ERROR("Bus %u already configured!", msg->bus);
        return;
    }
    LOG_INFO("Configuring Bus %u...", msg->bus);

    struct pio_uart *pio_uart = get_pio_uart_by_index(msg->bus);
    if (pio_uart == NULL || pio_uart->super.id != msg->bus)
    {
        LOG_ERROR("Invalid Bus number %u!", msg->bus);
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
        const struct m_base *msg_pr = &msg->periodic_reads[i];

        bus_pr->slave = msg_pr->slave;
        bus_pr->function = msg_pr->function;
        bus_pr->address = msg_pr->address;
        bus_pr->length = msg_pr->length;
        bus_pr->last_run = xTaskGetTickCount();
        bus_pr->memory_map_size = modbus_function_return_size(bus_pr->function, bus_pr->length);
        bus_pr->memory_map = pvPortMalloc(bus_pr->memory_map_size);
        memset(bus_pr->memory_map, 0, bus_pr->memory_map_size);
        LOG_DEBUG("Bus PR read 0x%" PRIx8 " 0x%" PRIx8 " 0x%" PRIx16 " 0x%" PRIx16, bus_pr->slave, bus_pr->function, bus_pr->address, bus_pr->length);
    }

    bus_init(bus_context);
}

void handle_m_read(const struct m_read *msg)
{
    xQueueSend(bus_read_queue, msg, NO_DELAY);
}

void handle_m_write(const struct m_write *msg)
{
    xQueueSend(bus_write_queue, msg, NO_DELAY);
}

void handle_m_pico_reset(const uint8_t *msg)
{
    (void)msg;
    LOG_ERROR("Pico Resetting...");
    watchdog_enable(1, 1);
    while (true)
        ;
}

void handle_m_heartbeat(const uint8_t *msg)
{
    (void)msg;
    LOG_DEBUG("Heartbeat received!");
    last_heartbeat_received = xTaskGetTickCount();
}

// Handle Min Packet received
void min_application_handler(uint8_t min_id, uint8_t const *min_payload, uint8_t len_payload, uint8_t port)
{
    (void)port;

#if LOG_LEVEL >= DEBUG_LEVEL
    static char hex_string[256];
    hex_string[0] = 0;
    for (uint8_t i = 0; i < len_payload; i++)
    {
        snprintf(&hex_string[i * 3], 4, "%02X ", min_payload[i]);
    }
    LOG_DEBUG("Min packet received: ID %u, Size: %u, Payload: %s", min_id, len_payload, hex_string);
#else
    (void)len_payload;
#endif

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

    struct m_change change;
    uint8_t read_byte;

    // Send Pico is alive message
    min_send_frame(&min_ctx, MESSAGE_PICO_READY_ID, NULL, 0);

    TickType_t last_heartbeat_sent = xTaskGetTickCount();
    last_heartbeat_received = xTaskGetTickCount();
    for (;;) // Task infinite loop
    {
        // Pool UART for incomming bytes to min protocol
        if (hw_uart_read_byte(&HOST_UART, &read_byte))
        {
            min_poll(&min_ctx, &read_byte, 1);
        }

        // Check if there is any change in the queue to be sent to host
        if (xQueueReceive(host_change_queue, &change, 0) == pdTRUE)
        {
            LOG_DEBUG("Sending Change Slave: %u Addr: %u Value: %04X",
                      change.base.slave, change.base.address, change.data);

            min_send_frame(&min_ctx, MESSAGE_CHANGE_ID, (uint8_t *)&change, sizeof(struct m_change));
        }

        if (xTaskGetTickCount() - last_heartbeat_sent > pdMS_TO_TICKS(HOST_HEARTBEAT_INTERVAL))
        {
            min_send_frame(&min_ctx, MESSAGE_HEARTBEAT_ID, NULL, 0);
            last_heartbeat_sent = xTaskGetTickCount();
        }

        if (xTaskGetTickCount() - last_heartbeat_received > pdMS_TO_TICKS(HOST_MAX_HEARTBEAT_INTERVAL))
        {
            LOG_ERROR("Heartbeat not received in %u ms!", HOST_MAX_HEARTBEAT_INTERVAL);
            handle_m_pico_reset(NULL);
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void host_init(void)
{
    LOG_DEBUG("Initializing Host...");

    hw_uart_init(&HOST_UART);

    min_init_context(&min_ctx, 0);

    host_change_queue = xQueueCreate(HOST_QUEUE_LENGTH, sizeof(struct m_change));

    xTaskCreate(task_host_handler, "Host Handler", configMINIMAL_STACK_SIZE, NULL, tskDEFAULT_PRIORITY, NULL);
}
