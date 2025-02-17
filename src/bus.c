
#include <FreeRTOS.h>
#include <task.h>

#include "macrologger.h"

#include "bus.h"
#include "host.h"
#include "utils.h"

char *name[COUNT_PIO_UARTS] = {
    "Bus 0",
    "Bus 1",
    "Bus 2",
    "Bus 3",
    "Bus 4",
    "Bus 5",
};

static struct bus_context *bus_contexts[COUNT_PIO_UARTS] = {NULL};

static bool send_modbus_frame(uint8_t bus, struct pio_uart *uart, uint8_t slave, uint8_t address, uint8_t *tx_frame, size_t frame_size, struct modbus_frame *rx_frame)
{
    struct modbus_parser parser;
    uint8_t read_byte = 0xaa;
    TickType_t last_timeout = 0;

    pio_uart_rx_flush(uart);                          // Flush any remaining byte in the UART RX buffer
    pio_uart_write_bytes(uart, tx_frame, frame_size); // Write the frame to the UART

    // Release the CPU until the frame is sent to UART and possibly the response is available
    vTaskDelay(pdMS_TO_TICKS(BUS_DELAY_WRITE_READ));

    modbus_parser_reset(&parser); // Reset the parser

    TickType_t timeout_max_tick = NEXT_TIMEOUT(BUS_TIMEOUT_RESPONSE); // Start timeout counter

    while (true)
    {
        if (!pio_uart_read_byte(uart, &read_byte)) // If there is no byte available, wait or timeout
        {

            if (IS_EXPIRED(timeout_max_tick)) // Check if the timeout has expired
            {
                if (IS_EXPIRED(last_timeout)) // Check if we need to print a timeout message
                {
                    LOG_ERROR(DEV_FMT "Timeout", bus, slave, address);
                    last_timeout = NEXT_TIMEOUT(BUS_DELAY_TIMEOUT_MSG);
                }
                // Go to next module if timeout
                break; // while, process next module
            }
            // Release CPU until some byte arrive in the UART
            vTaskDelay(pdMS_TO_TICKS(1)); // Minimum delay

            continue; // while, process next module
        }

        // Process parser result
        enum modbus_result parser_status = modbus_parser_process_byte(&parser, rx_frame, read_byte);
        if (parser_status == MODBUS_ERROR)
        {
            LOG_ERROR(DEV_FMT "Error parsing Modbus Frame", bus, slave, address);
            break; // while, process next module
        }
        else if (parser_status == MODBUS_COMPLETE)
        {
#ifdef BUS_DEBUG_MODBUS_FRAME
            LOG_DEBUG(DEV_FMT "Modbus Frame: %s",
                      bus, slave, address, to_hex_string(rx_frame->data, rx_frame->data_size));
#endif
            return true;
        }
    }
    return false;
}

static void bus_task(void *arg)
{
    struct bus_context *bus_context = arg;

    uint8_t tx_frame[BUS_MODBUS_FRAME_BUFFER_SIZE];
    struct modbus_frame rx_frame;
    size_t tx_frame_size = 0;

    // Update baud if present
    if (bus_context->baudrate > 0)
    {
        bus_context->pio_uart->super.baudrate = bus_context->baudrate;
    }
    // UART Initialization
    pio_uart_init(bus_context->pio_uart);

    for (;;) // Task infinite loop
    {
        // Handle Periodic reads
        // Iterate over all entries, and process them sequentially
        for (size_t i = 0; i < bus_context->periodic_reads_len; i++)
        {
            struct bus_periodic_read *p_read = &bus_context->periodic_reads[i];
            if (IS_EXPIRED(p_read->next_run))
            {
#ifdef BUS_DEBUG_PERIODIC_READS
                LOG_DEBUG(DEVF_FMT, "Periodic Read",
                          bus_context->bus, p_read->slave, p_read->address, p_read->function);
#endif
                tx_frame_size = modbus_create_read_frame(
                    p_read->function,
                    p_read->slave,
                    p_read->address,
                    tx_frame,
                    sizeof(tx_frame));
                if (tx_frame_size == 0)
                {
                    LOG_ERROR(DEVF_FMT "Modbus Frame creation failed",
                              bus_context->bus, p_read->slave, p_read->address, p_read->function);
                }
                else
                {
                    if (send_modbus_frame(
                            bus_context->bus,
                            bus_context->pio_uart,
                            p_read->slave,
                            p_read->address,
                            tx_frame,
                            tx_frame_size,
                            &rx_frame))
                    {
                        if (rx_frame.function_code != p_read->function)
                        {
                            LOG_ERROR(DEVF_FMT "Modbus Frame wrong function code %02X",
                                      bus_context->bus, p_read->slave, p_read->address, p_read->function,
                                      rx_frame.function_code);
                        }
                        else
                        {
                            process_periodic_reply(bus_context->bus, p_read, &rx_frame);
                        }
                    }
                }
                p_read->next_run = NEXT_TIMEOUT(bus_context->periodic_interval);
            }
            taskYIELD();
        }

        //
        // Handle Commands
        struct m_command command;
        if (xQueueReceive(bus_context->command_queue, &command, QUEUE_NO_WAIT))
        {
            struct m_device device = command.device;
            LOG_DEBUG(DEVF_FMT "Processing Command - Type: %u, Seq: %u",
                      bus_context->bus, device.slave, device.address, device.function,
                      command.type, command.seq);

            switch (command.type)
            {
            case MESSAGE_COMMAND_READ:
                tx_frame_size = modbus_create_read_frame(
                    device.function,
                    device.slave,
                    device.address,
                    tx_frame,
                    sizeof(tx_frame));
                break;
            case MESSAGE_COMMAND_WRITE:
                tx_frame_size = modbus_create_write_frame(
                    device.function,
                    device.slave,
                    device.address,
                    command.msg.write.data,
                    tx_frame,
                    sizeof(tx_frame));
                break;
            default:
                tx_frame_size = 0;
                LOG_ERROR(DEVF_FMT "Modbus Frame invalid command type %u",
                          bus_context->bus, device.slave, device.address, device.function,
                          command.type);
                break;
            }
            if (tx_frame_size == 0)
            {
                LOG_ERROR(DEVF_FMT "Modbus Frame creation failed",
                          bus_context->bus, device.slave, device.address, device.function);
            }
            else
            {
                struct m_command reply = {
                    .device = device,
                    .seq = command.seq,
                };
                // Set reply in case of failure
                switch (command.type)
                {
                case MESSAGE_COMMAND_READ:
                    reply.type = MESSAGE_COMMAND_READ_REPLY;
                    reply.msg.read_reply.done = false;
                    reply.msg.read_reply.data = 0;
                    break;
                case MESSAGE_COMMAND_WRITE:
                    reply.type = MESSAGE_COMMAND_WRITE_REPLY;
                    reply.msg.write_reply.done = false;
                    break;
                }
                if (send_modbus_frame(
                        bus_context->bus,
                        bus_context->pio_uart,
                        device.slave,
                        device.address,
                        tx_frame,
                        tx_frame_size,
                        &rx_frame))
                {
                    if (rx_frame.function_code != device.function)
                    {
                        LOG_ERROR(DEVF_FMT "Modbus Frame wrong function code %02X",
                                  bus_context->bus, device.slave, device.address, device.function,
                                  rx_frame.function_code);
                    }
                    else
                    {
                        switch (command.type)
                        {
                        case MESSAGE_COMMAND_READ:
                            reply.msg.read_reply.done = true;
                            reply.msg.read_reply.data = rx_frame.data[0] << 8 | rx_frame.data[1];
                            break;
                        case MESSAGE_COMMAND_WRITE:
                            reply.msg.write_reply.done = true;
                            break;
                        }
                    }
                }
                else
                {
                    LOG_ERROR(DEVF_FMT "Modbus Frame send failed",
                              bus_context->bus, device.slave, device.address, device.function);
                }
                if (!xQueueSend(host_command_queue, &reply, QUEUE_NO_WAIT))
                {
                    LOG_ERROR("Bus %u could not send read reply to queue, queue full!", bus_context->bus);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1)); // Minimum delay
    }
}

void process_periodic_reply(uint8_t bus, struct bus_periodic_read *p_read, struct modbus_frame *frame)
{
    if (frame->data_size != 2)
    {
        LOG_ERROR("Error Modbus Frame Data size must be equal to 2!");
        return;
    }

    struct m_command reply = {0};

    reply.device.bus = bus;
    reply.device.slave = p_read->slave;
    reply.device.function = p_read->function;
    reply.device.address = p_read->address;
    reply.msg.periodic_change.data = frame->data[0] << 8 | frame->data[1];
    reply.msg.periodic_change.data_mask = frame->data[0] ^ p_read->last_data;
    // If there is any change, send it to the host
    if (reply.msg.periodic_change.data_mask)
    {
#ifdef BUS_DEBUG_PERIODIC_READS
        LOG_INFO(DEVF_FMT "Change detected %04x",
                 bus, reply.device.slave, reply.device.address,
                 to_bin_hex_string(&reply.msg.periodic_change.data, 2));
#endif
        if (!xQueueSend(host_change_queue, &reply, QUEUE_NO_WAIT))
        {
            LOG_ERROR("Bus %u could not send change to queue, queue full!", bus);
        }
    }
    // Copy new read values to the last_values array
    p_read->last_data = reply.msg.periodic_change.data;
}

void bus_init(struct bus_context *bus_context)
{
    bus_contexts[bus_context->bus] = bus_context;

    BaseType_t result = xTaskCreate(bus_task,
                                    name[bus_context->bus],
                                    configMINIMAL_STACK_SIZE * 4,
                                    bus_context,
                                    tskDEFAULT_PRIORITY,
                                    NULL);

    if (result != pdPASS)
    {
        LOG_ERROR("Bus Task creation failed!");
    }
}

struct bus_context *bus_get_context(uint8_t bus)
{
    return bus_contexts[bus];
}
