#include "macrologger.h"

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

// Just to make it clear that 0 means no delay in StreamBuffer calls
static const uint8_t NO_DELAY = 0;

static struct bus_context *bus_contexts[COUNT_PIO_UARTS] = {NULL};

// After how many ms we will print the timeout message again
static const TickType_t DELAY_TIMEOUT_MSG = 5000;
// After how many ms we will print the timeout message again
static const TickType_t TIMEOUT_RESPONSE = 50;

QueueHandle_t bus_read_queue;
QueueHandle_t bus_write_queue;

static TickType_t delay_write_read;

static struct modbus_frame *send_modbus_frame(uint8_t bus, struct pio_uart *uart, uint8_t slave, uint8_t address, uint8_t *frame, size_t frame_size)
{
    static struct modbus_parser parser;
    static struct modbus_frame parser_frame;
    static uint8_t read_byte = 0xaa;
    static TickType_t last_timeout;

    last_timeout = 0;

    // Flush any remaining byte in the UART RX buffer
    pio_uart_rx_flush(uart);
    // Write the frame to the UART
    pio_uart_write_bytes(uart, frame, frame_size);

    // Release the CPU until the frame is sent to UART and possibly the response is available
    vTaskDelay(delay_write_read);

    // Reset the parser
    modbus_parser_reset(&parser);
    // Get current tick, needed for the timeout
    TickType_t timeout_max_tick = NEXT_TIMEOUT(TIMEOUT_RESPONSE);
    while (true)
    {
        // If there is no byte available, wait or timeout
        if (!pio_uart_read_byte(uart, &read_byte))
        {
            // Check if the timeout has expired
            if (IS_EXPIRED(timeout_max_tick))
            {
                if (IS_EXPIRED(last_timeout))
                {
                    LOG_ERROR("Bus:%u Timeout on Slave: %d Addr: %d", bus, slave, address);

                    last_timeout = NEXT_TIMEOUT(DELAY_TIMEOUT_MSG);
                }
                // Go to next module if timeout
                break; // while, process next module
            }
            // Release CPU until some byte arrive in the buffer
            vTaskDelay(pdMS_TO_TICKS(1));
            continue; // while, process next module
        }

        // Process parser result
        enum modbus_result parser_status = modbus_parser_process_byte(&parser, &parser_frame, read_byte);
        if (parser_status == MODBUS_ERROR)
        {
            LOG_ERROR("Bus %u Error parsing frame for Slave: %d, Addr: %d", bus, slave, address);
            break; // while, process next module
        }
        else if (parser_status == MODBUS_COMPLETE)
        {
            // Just for debug
            // LOG_DEBUG("Bus %u Received - Slave: %d, Addr: %d, Length: %d, Values: ",
            //        bus, slave, address, parser_frame.data_size);
            // for (size_t i = 0; i < parser_frame.data_size; i++)
            // {
            //     LOG_DEBUG("%02X ", parser_frame.data[i]);
            // }
            // debug end

            return &parser_frame;
        }
    }
    return NULL;
}

static void bus_task(void *arg)
{
    struct bus_context *bus_context = arg;

    uint8_t framer_frame[BUS_MODBUS_FRAME_BUFFER_SIZE];
    size_t framer_frame_size = 0;

    // UART Initialization
    if (bus_context->baudrate > 0)
    {
        bus_context->pio_uart->super.baudrate = bus_context->baudrate;
    }
    pio_uart_init(bus_context->pio_uart);

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
                LOG_DEBUG("Bus %u Periodic read %u", bus_context->bus, i);

                framer_frame_size = modbus_create_multiple_read_frame(p_read->function, p_read->slave, p_read->address, p_read->length, framer_frame, sizeof(framer_frame));
                if (framer_frame_size == 0)
                {
                    LOG_ERROR("Bus %u Could not create Modbus Frame", bus_context->bus);
                    continue;
                }

                struct modbus_frame *frame = send_modbus_frame(bus_context->bus, bus_context->pio_uart, p_read->slave, p_read->address, framer_frame, framer_frame_size);
                if (frame->function_code != p_read->function)
                {
                    LOG_ERROR("Bus %u Received frame with wrong function code for Slave: %d, Addr: %d",
                              bus_context->bus, p_read->slave, p_read->address);
                    break; // while, process next module
                }

                process_modbus_response(bus_context->bus, p_read, frame);

                p_read->last_run = xTaskGetTickCount();
            }
            taskYIELD();
        }

        //
        // Handle arbitrary Reads

        struct m_read read;
        if (xQueueReceive(bus_read_queue, &read, NO_DELAY) == pdTRUE)
        {
            LOG_DEBUG("Bus %u Arbitrary Read", bus_context->bus);

            framer_frame_size = modbus_create_multiple_read_frame(read.base.function, read.base.slave, read.base.address, read.base.length, framer_frame, sizeof(framer_frame));
            if (framer_frame_size == 0)
            {
                LOG_ERROR("Bus %u Could not create Modbus Frame", bus_context->bus);
                continue;
            }
            struct modbus_frame *frame = send_modbus_frame(bus_context->bus, bus_context->pio_uart, read.base.slave, read.base.address, framer_frame, framer_frame_size);
            if (frame->function_code != read.base.function)
            {
                LOG_ERROR("Bus %u Received frame with wrong function code for Slave: %d, Addr: %d",
                          bus_context->bus, read.base.slave, read.base.address);
                break; // while, process next module
            }
        }

        //
        // Handle arbitrary Writes

        struct m_write write;
        if (xQueueReceive(bus_write_queue, &write, NO_DELAY) == pdTRUE)
        {
            LOG_DEBUG("Bus %u Arbitrary Write", bus_context->bus);

            framer_frame_size = modbus_create_multiple_write_frame(write.base.function, write.base.slave, write.base.address, write.base.length, framer_frame, sizeof(framer_frame));
            if (framer_frame_size == 0)
            {
                LOG_ERROR("Bus %u Could not create Modbus Frame", bus_context->bus);
                continue;
            }
            struct modbus_frame *frame = send_modbus_frame(bus_context->bus, bus_context->pio_uart, write.base.slave, write.base.address, framer_frame, framer_frame_size);
            if (frame->function_code != write.base.function)
            {
                LOG_ERROR("Bus %u Received frame with wrong function code for Slave: %d, Addr: %d",
                          bus_context->bus, write.base.slave, write.base.address);
                break; // while, process next module
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void process_modbus_response(uint8_t bus, struct bus_periodic_read *p_read, struct modbus_frame *frame)
{
    // We reuse the same variable because it will be copied to the queue in case of a change
    static struct m_change change = {
        .base = {
            .slave = 0,
            .function = 0,
            .address = 0,
            .length = 0,
        },
        .data = 0,
    };

    size_t data_size = modbus_function_register_size(frame->function_code);
    if (frame->data_size % 2 != 0)
    {
        LOG_ERROR("Error data size must be even");
        return;
    }

    for (size_t byte = 0; byte < frame->data_size; ++byte)
    {
        uint8_t diff = frame->data[byte] ^ p_read->memory_map[byte]; // XOR to find if there is any change

        if (diff) // If there is any change in the byte...
        {
            if (data_size == 16) // ...assume that the whole word changed
            {
                change.base.address = frame->address + byte / 2;
                change.data = frame->data[byte] << 8 | frame->data[byte + 1];
            }
            else if (data_size == 1) // ...search for the bit that changed
            {
                for (uint8_t bit = 0; bit < 8; ++bit)
                {
                    if (diff & (1 << bit)) // Change detected
                    {
                        change.base.address = frame->address + (byte * 8 + bit);
                        change.data = (frame->data[byte] & (1 << bit)) ? 1 : 0;
                    }
                }
            }
            else
            {
                LOG_ERROR("Invalid data_size %u on Bus %u.", data_size, bus);
                return;
            }

            change.bus = bus;
            change.base.slave = p_read->slave;
            change.base.function = frame->function_code;
            change.base.length = 1;
            LOG_INFO("Bus %u Change detected - Slave: %d, Addr: %02d = %04X",
                     bus, change.base.slave, change.base.address, change.data);
            if (xQueueSend(host_change_queue, &change, NO_DELAY) != pdTRUE)
            {
                LOG_ERROR("Bus %u could not send change to queue, queue full!", bus);
            }
        }
        if (data_size == 16)
            byte++; // go to the next word
    }

    // Copy new read values to the last_values array
    memcpy(p_read->memory_map, frame->data, frame->data_size);
}

void bus_init(struct bus_context *bus_context)
{
    bus_contexts[bus_context->bus] = bus_context;

    bus_read_queue = xQueueCreate(HOST_QUEUE_LENGTH, sizeof(struct m_read));
    bus_write_queue = xQueueCreate(HOST_QUEUE_LENGTH, sizeof(struct m_write));
    // How long to wait between writing and reading(Using current baudrate, 10 bytes)
    delay_write_read = pdMS_TO_TICKS(((100 * 1000000) / bus_context->pio_uart->super.baudrate + 999) / 1000);

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
