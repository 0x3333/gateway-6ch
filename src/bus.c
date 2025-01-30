#include <stdio.h>
#include <FreeRTOS.h>
#include <task.h>
#include <nanomodbus.h>

#include "bus.h"
#include "debug.h"

char *name[COUNT_PIO_UARTS] = {
    "Bus 1",
    "Bus 2",
    "Bus 3",
    "Bus 4",
    "Bus 5",
    "Bus 6",
};

static struct bus_context *bus_contexts[COUNT_PIO_UARTS] = {NULL};
// TaskHandle_t taskHandlers[COUNT_PIO_UARTS] = {NULL};

int32_t read_serial(uint8_t *buf, uint16_t count, int32_t byte_timeout_ms, void *arg)
{
    struct pio_uart *pio_uart = arg;

    if (byte_timeout_ms == 0) // This is a flush
    {
        pio_uart_read_bytes(pio_uart, buf, count);
        return 0;
    }

    size_t read = 0;
    uint8_t *dst = buf;
    TickType_t timeout = pdMS_TO_TICKS((byte_timeout_ms));
    TickType_t timeout_inc = pdMS_TO_TICKS(1);
    while (read < count)
    {
        read += pio_uart_read_bytes_timeout(pio_uart, dst, count - read, timeout_inc);
        dst += read * sizeof(uint8_t);

        timeout -= timeout_inc;
        if (timeout <= 0)
        {
            break;
        }
    }
    if (read != count)
        printf("Fail reading: %u of %u, RX: %u\n", read, count, xStreamBufferBytesAvailable(pio_uart->super.rx_sbuffer));
    return read;
}

int32_t write_serial(const uint8_t *buf, uint16_t count, int32_t byte_timeout_ms, void *arg)
{
    (void)byte_timeout_ms; // No timeouts on write, we have the bus
    struct pio_uart *pio_uart = arg;
    size_t read = pio_uart_write_bytes(pio_uart, buf, count);
    return read;
}

//
// Handle Bus management, sending and receiving modbus messages

static void bus_task(void *arg)
{
    struct bus_context *bus_context = arg;

    nmbs_platform_conf platform_conf;
    nmbs_platform_conf_create(&platform_conf);
    platform_conf.transport = NMBS_TRANSPORT_RTU;
    platform_conf.read = read_serial;
    platform_conf.write = write_serial;

    nmbs_t nmbs;
    nmbs_error err = nmbs_client_create(&nmbs, &platform_conf);
    if (err != NMBS_ERROR_NONE)
    {
        printf("Error creating Modbus client: %s\n", nmbs_strerror(err));
        return;
    }

    nmbs_set_platform_arg(&nmbs, bus_context->pio_uart);
    nmbs_set_read_timeout(&nmbs, BUS_TIMEOUT);
    nmbs_set_byte_timeout(&nmbs, BUS_TIMEOUT);

    // Initialization
    if (bus_context->baudrate > 0)
    {
        bus_context->pio_uart->super.baudrate = bus_context->baudrate;
    }
    pio_uart_init(bus_context->pio_uart);

    while (true)
    {
        TickType_t currentTicks = xTaskGetTickCount();

        // Handle Periodic reads
        // Iterate over all entries, and process them sequentially
        for (size_t i = 0; i < bus_context->periodic_reads_len; i++)
        {
            struct bus_periodic_read *p_read = &bus_context->periodic_reads[i];
            if (currentTicks >= p_read->last_run + bus_context->periodic_interval)
            {
                // printf("Periodic read %u on Bus %u\n", i, bus_context->bus);

                nmbs_error err = NMBS_ERROR_NONE;
                nmbs_set_destination_rtu_address(&nmbs, p_read->slave);
                memset(p_read->memory_map, 0, p_read->memory_map_size);
                if (p_read->function == MB_FUNC_READ_COILS)
                {
                    err = nmbs_read_coils(&nmbs, p_read->address, p_read->length, p_read->memory_map);
                }
                else if (p_read->function == MB_FUNC_READ_HOLDING_REGISTERS)
                {
                    err = nmbs_read_holding_registers(&nmbs, p_read->address, p_read->length, (uint16_t *)p_read->memory_map);
                }
                else
                {
                    printf("Invalid Modbus function %u on Bus %u.\n", p_read->function, bus_context->bus);
                    continue;
                }

                if (err != NMBS_ERROR_NONE)
                {
                    printf("Failed to read Modbus on Bus %u, error %d.\n", bus_context->bus, err);
                }
                // else
                // {
                //     printf("OK\n");
                // }
                p_read->last_run = xTaskGetTickCount();
            }
        }

        // Handle arbitrary read/write
        // TODO: When we receive a m_read/m_write message, dispatch a message in the modbus bus and on response, send the message back to the host

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void bus_init(struct bus_context *bus_context)
{
    bus_contexts[bus_context->bus] = bus_context;
    BaseType_t result = xTaskCreate(bus_task,
                                    name[bus_context->bus],
                                    configMINIMAL_STACK_SIZE,
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

size_t bits_to_bytes(size_t bits)
{
    return (bits + 7) / 8;
}

size_t bus_mb_function_to_map_size(enum mb_function func, size_t len)
{
    size_t ret = 0;
    ret = func == MB_FUNC_READ_COILS || func == MB_FUNC_WRITE_COILS ? 1 :                            // Coils = 1bit
              func == MB_FUNC_READ_HOLDING_REGISTERS || func == MB_FUNC_WRITE_HOLDING_REGISTERS ? 16 // Registers 16 bits
                                                                                                : 0; // Fallback
    return bits_to_bytes(ret * len);
}