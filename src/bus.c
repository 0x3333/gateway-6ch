#include <stdio.h>
#include <string.h>

#include "debug.h"
#include "bus.h"
#include "host.h"

#include <task.h>

// Tick helper functions
#define NEXT_TIMEOUT(timeout) (xTaskGetTickCount() + pdMS_TO_TICKS(timeout))
#define IS_EXPIRED(timeout) (xTaskGetTickCount() >= timeout)

char *name[COUNT_PIO_UARTS] = {
    "Bus 1",
    "Bus 2",
    "Bus 3",
    "Bus 4",
    "Bus 5",
    "Bus 6",
};

static struct bus_context *bus_contexts[COUNT_PIO_UARTS] = {NULL};

// // Delay between loops, allow cmds to be processed
// static const TickType_t LOOP_DELAY = 0; // 0 means yeld
// // Delay after reading a module before reading the next one
// static const TickType_t MODULE_DELAY = 0; // 0 means yeld
// // Delay between reading all modules and starting again
// static const TickType_t LOOP_MODULE_DELAY = 50; // This needs to be validated on hardware modules

// After how many ms we will print the timeout message again
static const TickType_t DELAY_TIMEOUT_MSG = 5000;
// After how many ms we will print the timeout message again
static const TickType_t TIMEOUT_RESPONSE = 50;

//
// Handle Bus management, sending and receiving modbus messages

static void bus_task(void *arg)
{
    struct bus_context *bus_context = arg;

    uint8_t framer_frame[BUS_MODBUS_FRAME_BUFFER_SIZE];
    size_t frame_size = 0;
    struct ModbusParser parser;
    struct ModbusFrame parser_frame;
    uint8_t read_byte = 0xaa;

    TickType_t last_timeout = 0;

    // UART Initialization
    if (bus_context->baudrate > 0)
    {
        bus_context->pio_uart->super.baudrate = bus_context->baudrate;
    }
    pio_uart_init(bus_context->pio_uart);

    // How long to wait between writing and reading(Using current baudrate, 10 bytes)
    const TickType_t delay_write_read = pdMS_TO_TICKS(
        ((100 * 1000000) / bus_context->pio_uart->super.baudrate + 999) / 1000);

    for (;;) // Task infinite loop
    {
        TickType_t current_ticks = xTaskGetTickCount();

        // Handle Periodic reads
        // Iterate over all entries, and process them sequentially
        for (size_t i = 0; i < bus_context->periodic_reads_len; i++)
        {
            struct bus_periodic_read *p_read = &bus_context->periodic_reads[i];
            if (current_ticks >= p_read->last_run + bus_context->periodic_interval)
            {
                printf("Periodic read %u on Bus %u\n", i, bus_context->bus);

                if (p_read->function == MODBUS_FUNCTION_READ_COILS)
                {
                    frame_size = modbus_create_read_coils_frame(p_read->slave, p_read->address, p_read->length, framer_frame, sizeof(framer_frame));
                    if (frame_size == 0)
                    {
                        printf("Could not create Modbus Frame on Bus %u.\n", bus_context->bus);
                        continue;
                    }
                }
                else if (p_read->function == MODBUS_FUNCTION_READ_HOLDING_REGISTERS)
                {
                    frame_size = modbus_create_read_holding_registers_frame(p_read->slave, p_read->address, p_read->length, framer_frame, sizeof(framer_frame));
                    if (frame_size == 0)
                    {
                        printf("Could not create Modbus Frame on Bus %u.\n", bus_context->bus);
                        continue;
                    }
                }
                else
                {
                    printf("Invalid Modbus function %u on Bus %u.\n", p_read->function, bus_context->bus);
                    continue;
                }

                // Flush any remaining byte in the UART RX buffer
                pio_uart_flush_rx(bus_context->pio_uart);
                // Write the frame to the UART
                pio_uart_write_bytes(bus_context->pio_uart, framer_frame, frame_size);

                // Release the CPU until the frame is sent to UART and possibly the response is available
                vTaskDelay(delay_write_read);

                // Reset the parser
                modbus_parser_reset(&parser);
                // Get current tick, needed for the timeout
                TickType_t timeout_max_tick = NEXT_TIMEOUT(TIMEOUT_RESPONSE);
                while (true)
                {
                    // If there is no byte available, wait or timeout
                    if (!pio_uart_read_byte(bus_context->pio_uart, &read_byte))
                    {
                        // Check if the timeout has expired
                        if (IS_EXPIRED(timeout_max_tick))
                        {
                            if (IS_EXPIRED(last_timeout))
                            {
                                printf("Timeout on Bus:%u Slave:%d Addr:%d Tick:%lu\n",
                                       bus_context->bus, p_read->slave, p_read->address, xTaskGetTickCount());

                                last_timeout = NEXT_TIMEOUT(DELAY_TIMEOUT_MSG);
                            }
                            // Go to next module if timeout
                            break; // while, process next module
                        }
                        // Release CPU until some byte arrive in the buffer
                        vTaskDelay(pdMS_TO_TICKS(1));
                        continue; // while, process next module
                    }

                    printf("Byte: %02X\n", read_byte);
                    // Process parser result
                    enum ModbusResult parser_status = modbus_parser_process_byte(&parser, &parser_frame, read_byte);
                    if (parser_status == MODBUS_ERROR)
                    {
                        printf("Bus %u error parsing frame for Slave ID: %d, Address: %d\n",
                               bus_context->bus, p_read->slave, p_read->address);
                        break; // while, process next module
                    }
                    else if (parser_status == MODBUS_COMPLETE)
                    {
                        if (parser_frame.function_code != p_read->function)
                        {
                            printf("Bus %u received frame with wrong function code for Slave ID: %d, Address: %d\n",
                                   bus_context->bus, p_read->slave, p_read->address);
                            break; // while, process next module
                        }

                        // Just for debug
                        printf("Bus %u Received - Slave ID: %d, Address: %d, Length: %d, Values: ",
                               bus_context->bus, p_read->slave, p_read->address, p_read->length);
                        for (size_t i = 0; i < parser_frame.data_size; i++)
                        {
                            printf("%02X ", parser_frame.data[i]);
                        }
                        printf("\n");
                        // debug end

                        process_modbus_response(bus_context->bus, p_read, &parser_frame);
                        break; // while, process next module
                    }
                }

                p_read->last_run = xTaskGetTickCount();
            }
            vTaskDelay(pdMS_TO_TICKS(1));
        }

        // Handle arbitrary read/write
        // TODO: When we receive a m_read/m_write message, dispatch a message in the modbus bus and on response, send the message back to the host

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void process_modbus_response(uint8_t bus, struct bus_periodic_read *p_read, struct ModbusFrame *frame)
{
    static struct modbus_change change = {
        .slave_id = 0,
        .address = 0,
        .value = 0,
    };

    // Compare received values from the last ones
    for (size_t i = 0; i < frame->data_size; ++i)
    {
        uint8_t diff = frame->data[i] ^ p_read->memory_map[i];
        if (diff)
        {
            for (uint8_t bit = 0; bit < 8; ++bit)
            {
                if (diff & (1 << bit))
                {
                    uint8_t value = (frame->data[i] & (1 << bit)) ? 1 : 0;
                    change.bus = bus;
                    change.slave_id = p_read->slave;
                    change.address = i * 8 + bit;
                    change.value = value;
                    printf("Bus %u change detected - Slave ID: %d, Address: %02d = %d",
                           bus, p_read->slave, change.address, change.value);
                    if (xQueueSend(host_change_queue, &change, 0) != pdTRUE)
                    {
                        printf("Bus %u could not send change to queue, queue full!", bus);
                    }
                }
            }
        }
    }
    // Copy new read values to the last_values array
    memcpy(p_read->memory_map, frame->data, frame->data_size);
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
        printf("Bus Task creation failed!");
    }
}

struct bus_context *bus_get_context(uint8_t bus)
{
    return bus_contexts[bus];
}
