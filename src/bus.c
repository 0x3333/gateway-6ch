#include "macrologger.h"

#include "bus.h"
#include "host.h"

#include <task.h>

// Tick helper functions
#define NEXT_TIMEOUT(timeout) (xTaskGetTickCount() + pdMS_TO_TICKS(timeout))
#define IS_EXPIRED(timeout) (xTaskGetTickCount() >= timeout)

QueueHandle_t bus_read_queue;
QueueHandle_t bus_write_queue;

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
// How many ms we will wait after writing to UART before reading
static const TickType_t delay_write_read = 3;

static bool send_modbus_frame(uint8_t bus, struct pio_uart *uart, uint8_t slave, uint8_t address, uint8_t *tx_frame, size_t frame_size, struct modbus_frame *rx_frame)
{
    struct modbus_parser parser;
    uint8_t read_byte = 0xaa;
    TickType_t last_timeout = 0;

    // Flush any remaining byte in the UART RX buffer
    pio_uart_rx_flush(uart);
    // Write the frame to the UART
    pio_uart_write_bytes(uart, tx_frame, frame_size);
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
        enum modbus_result parser_status = modbus_parser_process_byte(&parser, rx_frame, read_byte);
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
            return true;
        }
    }
    return false;
}

static void bus_task(void *arg)
{
    struct bus_context *bus_context = arg;

    uint8_t tx_frame[BUS_MODBUS_FRAME_BUFFER_SIZE];
    size_t tx_frame_size = 0;

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

                tx_frame_size = modbus_create_read_frame(
                    p_read->function,
                    p_read->slave,
                    p_read->address,
                    tx_frame,
                    sizeof(tx_frame));

                if (tx_frame_size == 0)
                {
                    LOG_ERROR("Bus %u Could not create Modbus Frame", bus_context->bus);
                }
                else
                {
                    struct modbus_frame rx_frame;
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
                            LOG_ERROR("Bus %u Received frame with wrong function code for Slave: %d, Addr: %d",
                                      bus_context->bus, p_read->slave, p_read->address);
                        }
                        else
                        {
                            process_modbus_response(bus_context->bus, p_read, &rx_frame);
                        }
                    }
                }
                p_read->last_run = xTaskGetTickCount(); // Update even if it failed
            }
            taskYIELD();
        }

        //
        // Handle arbitrary Reads

        struct m_read read;
        if (xQueueReceive(bus_read_queue, &read, NO_DELAY) == pdTRUE)
        {
            LOG_DEBUG("Bus %u Arbitrary Read", bus_context->bus);

            tx_frame_size = modbus_create_read_frame(
                read.base.function,
                read.base.slave,
                read.base.address,
                tx_frame,
                sizeof(tx_frame));
            if (tx_frame_size == 0)
            {
                LOG_ERROR("Bus %u Could not create Modbus Frame", bus_context->bus);
            }
            else
            {
                struct modbus_frame rx_frame;
                if (send_modbus_frame(
                        bus_context->bus,
                        bus_context->pio_uart,
                        read.base.slave,
                        read.base.address,
                        tx_frame,
                        tx_frame_size,
                        &rx_frame))
                {
                }
                if (rx_frame.function_code != read.base.function)
                {
                    LOG_ERROR("Bus %u Received frame with wrong function code for Slave: %d, Addr: %d",
                              bus_context->bus, read.base.slave, read.base.address);
                }
            }
        }

        //
        // Handle arbitrary Writes

        struct m_write write;
        if (xQueueReceive(bus_write_queue, &write, NO_DELAY) == pdTRUE)
        {
            LOG_DEBUG("Bus %u Arbitrary Write", bus_context->bus);

            tx_frame_size = modbus_create_write_frame(
                write.base.function,
                write.base.slave,
                write.base.address,
                write.data,
                tx_frame,
                sizeof(tx_frame));
            if (tx_frame_size == 0)
            {
                LOG_ERROR("Bus %u Could not create Modbus Frame", bus_context->bus);
            }
            else
            {
                struct modbus_frame rx_frame;
                if (send_modbus_frame(
                        bus_context->bus,
                        bus_context->pio_uart,
                        read.base.slave,
                        read.base.address,
                        tx_frame,
                        tx_frame_size,
                        &rx_frame))
                {
                }
                if (rx_frame.function_code != read.base.function)
                {
                    LOG_ERROR("Bus %u Received frame with wrong function code for Slave: %d, Addr: %d",
                              bus_context->bus, write.base.slave, write.base.address);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void process_modbus_response(uint8_t bus, struct bus_periodic_read *p_read, struct modbus_frame *frame)
{
    struct m_read_response response = {0};

    if (frame->data_size != 2)
    {
        LOG_ERROR("Error Modbus Frame Data size must be equal to 2!");
        return;
    }

    response.bus = bus;
    response.base.slave = p_read->slave;
    response.base.function = p_read->function;
    response.base.address = p_read->address;
    response.data = frame->data[0];
    response.data_mask = frame->data[0] ^ p_read->last_data;
    // If there is any change, send it to the host
    if (response.data_mask)
    {
        LOG_INFO("Bus %u Change detected - Slave: %d, Addr: %02d = %04X",
                 bus, response.base.slave, response.base.address, response.data);
        if (xQueueSend(host_change_queue, &response, NO_DELAY) != pdTRUE)
        {
            LOG_ERROR("Bus %u could not send change to queue, queue full!", bus);
        }
    }
    // Copy new read values to the last_values array
    p_read->last_data = response.data;
}

void bus_init(struct bus_context *bus_context)
{
    bus_contexts[bus_context->bus] = bus_context;

    bus_read_queue = xQueueCreate(HOST_QUEUE_LENGTH, sizeof(struct m_read));
    bus_write_queue = xQueueCreate(HOST_QUEUE_LENGTH, sizeof(struct m_write));

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
