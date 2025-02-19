#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <hardware/watchdog.h>
#include <target/min.h>

#include "macrologger.h"

#include "host.h"
#include "uart.h"
#include "utils.h"
#include "messages.h"
#include "bus.h"

static struct min_context min_ctx;

static volatile TickType_t next_heartbeat;

QueueHandle_t host_change_queue;
QueueHandle_t host_command_queue;

//
// Messages
//

void handle_m_config_bus(const struct m_config_bus *msg)
{
    struct m_command reply = {0};
    reply.type = MESSAGE_CONFIG_BUS_REPLY;
    reply.msg.config_bus_reply.done = false;

    if (bus_get_context(msg->bus))
    {
        LOG_ERROR("Bus %u already configured!", msg->bus);
        reply.msg.config_bus_reply.already_configured = true;
        xQueueSend(host_command_queue, &reply, FREERTOS_NO_WAIT);
        return;
    }
    LOG_INFO("Bus %u starting", msg->bus);

    struct pio_uart *pio_uart = get_pio_uart_by_index(msg->bus);
    if (pio_uart == NULL || pio_uart->super.id != msg->bus)
    {
        LOG_ERROR("Invalid Bus number %u!", msg->bus);
        reply.msg.config_bus_reply.invalid_bus = true;
        xQueueSend(host_command_queue, &reply, FREERTOS_NO_WAIT);
        return;
    }

    // Calculate the size of all elements in the struct
    size_t bus_context_size = sizeof(struct bus_context) +
                              (sizeof(struct bus_periodic_read) * msg->periodic_reads_length);

    struct bus_context *bus_context = pvPortCalloc(1, bus_context_size);

    bus_context->pio_uart = pio_uart;
    bus_context->baudrate = msg->baudrate;
    bus_context->bus = msg->bus;
    bus_context->command_queue = xQueueCreate(HOST_QUEUE_LENGTH, sizeof(struct m_command));
    bus_context->periodic_interval = msg->periodic_interval;
    bus_context->periodic_reads_len = msg->periodic_reads_length;
    for (size_t i = 0; i < bus_context->periodic_reads_len; i++)
    {
        struct bus_periodic_read *bus_pr = &bus_context->periodic_reads[i];
        const struct m_device *msg_pr = &msg->periodic_reads[i];

        bus_pr->slave = msg_pr->slave;
        bus_pr->function = msg_pr->function;
        bus_pr->address = msg_pr->address;
        bus_pr->next_run = 0;
        bus_pr->last_data = 0;
        LOG_INFO(DEVF_FMT "Periodic Read", msg->bus, bus_pr->slave, bus_pr->address, bus_pr->function);
    }
    bus_init(bus_context);

    reply.msg.config_bus_reply.done = true;
    xQueueSend(host_command_queue, &reply, FREERTOS_NO_WAIT);
}

void handle_m_command(const struct m_command *msg)
{
    xQueueSend(bus_get_context(msg->device.bus)->command_queue, msg, FREERTOS_NO_WAIT);
}

void handle_m_pico_reset(const uint8_t *msg)
{
    (void)msg;
    LOG_ERROR("Pico Resetting");
    watchdog_enable(1, 1); // Enable watchdog and check how to work this out
    while (true)
        ;
}

/**
 * Lock the mutex to send the frame in one piece.
 */
static inline void safe_min_send_frame(uint8_t min_id, uint8_t const *payload, uint8_t payload_len)
{
    if (xSemaphoreTake(HOST_UART.super.tx_buffer_mutex, portMAX_DELAY))
    {
        min_send_frame(&min_ctx, min_id, payload, payload_len);
        xSemaphoreGive(HOST_UART.super.tx_buffer_mutex);
    }
}

// Handle Min Packet received
void min_application_handler(uint8_t min_id, uint8_t const *min_payload, uint8_t len_payload, uint8_t port)
{
    (void)port;
    (void)len_payload;
#ifdef HOST_DEBUG_MIN_FRAME
    LOG_DEBUG("Min packet received: ID %u, Size: %u, Payload: %s", min_id, len_payload, to_hex_string(min_payload, len_payload));
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

_Noreturn static void task_host_handler(void *arg)
{
    (void)arg;

    uint8_t read_byte;
    struct m_command command;

    // Send Pico is alive message
    safe_min_send_frame(MESSAGE_PICO_READY, NULL, 0);

    for (;;) // Task infinite loop
    {
        // Pool UART for incomming bytes to min protocol
        if (hw_uart_read_bytes(&HOST_UART, &read_byte, 1))
        {
            min_poll(&min_ctx, &read_byte, 1);
        }

        // Check if there is any change in the queue to be sent to host
        if (xQueueReceive(host_change_queue, &command, 0))
        {
            LOG_DEBUGD("Sending Change - %04X", &command.device, command.msg.read_reply.data);
            safe_min_send_frame(MESSAGE_PERIODIC_READ_REPLY, (uint8_t *)&command, sizeof(command));
        }

        // Check if there is any command response in the queue to be sent to host
        if (xQueueReceive(host_command_queue, &command, 0))
        {
            switch (command.type)
            {
            case MESSAGE_CONFIG_BUS_REPLY:
                LOG_DEBUGD("Sending Config Bus Reply Seq: %u Done: %c", &command.device, command.seq, LOG_BOOL(command.msg.read_reply.done));
                break;
            case MESSAGE_COMMAND_READ_REPLY:
                LOG_DEBUGD("Sending READ Reply Seq: %u Done: %c", &command.device, command.seq, LOG_BOOL(command.msg.read_reply.done));
                break;
            case MESSAGE_COMMAND_WRITE_REPLY:
                LOG_DEBUGD("Sending WRITE Reply Seq: %u Done: %c", &command.device, command.seq, LOG_BOOL(command.msg.write_reply.done));
                break;
            default:
                LOG_ERROR("Unknown command type %u", command.type);
                break;
            }
            safe_min_send_frame(command.type, (uint8_t *)&command, sizeof(command));
        }

        if (IS_EXPIRED(next_heartbeat))
        {
            safe_min_send_frame(MESSAGE_HEARTBEAT, NULL, 0);
            next_heartbeat = NEXT_TIMEOUT(HOST_HEARTBEAT_INTERVAL);
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void host_init(void)
{
    LOG_DEBUG("Initializing Host");

    hw_uart_init(&HOST_UART);

    min_init_context(&min_ctx, 0);

    host_change_queue = xQueueCreate(HOST_QUEUE_LENGTH, sizeof(struct m_command));
    host_command_queue = xQueueCreate(HOST_QUEUE_LENGTH, sizeof(struct m_command));

    xTaskCreateAffinitySet(task_host_handler,
                           "Host Handler",
                           configMINIMAL_STACK_SIZE * 2,
                           NULL,
                           tskDEFAULT_PRIORITY,
                           HOST_TASK_CORE_AFFINITY,
                           NULL);
}
