#include <stdio.h>
#include <FreeRTOS.h>
#include <task.h>
#include <nanomodbus.h>

#include "bus.h"

char *name[6] = {
    "Bus 1",
    "Bus 2",
    "Bus 3",
    "Bus 4",
    "Bus 5",
    "Bus 6",
};

TaskHandle_t taskHandlers[6] = {NULL};

int32_t read_serial(uint8_t *buf, uint16_t count, int32_t byte_timeout_ms, void *arg)
{
    struct bus_context *bus_context = arg;
    return pio_uart_read_bytes_timeout(bus_context->pio_uart, buf, count, pdMS_TO_TICKS(byte_timeout_ms));
}

int32_t write_serial(const uint8_t *buf, uint16_t count, int32_t byte_timeout_ms, void *arg)
{
    struct bus_context *bus_context = arg;
    return pio_uart_write_bytes_timeout(bus_context->pio_uart, buf, count, pdMS_TO_TICKS(byte_timeout_ms));
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
    }

    nmbs_set_platform_arg(&nmbs, bus_context);

    printf("Setting Modbus timeouts...\n");
    nmbs_set_read_timeout(&nmbs, 10);
    nmbs_set_byte_timeout(&nmbs, 1);

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
        for (size_t i = 0; i < bus_context->periodic_reads_size; i++)
        {
            struct periodic_reads p_read = bus_context->periodic_reads[i];

            if (p_read.last_run + bus_context->periodic_interval >= currentTicks)
            {
                nmbs_set_destination_rtu_address(&nmbs, p_read.slave);
                // We must create a memory map for each call
                // Each address must hold a single bit or a 16 bits register for the length specified by the periodic read
                if (p_read.function == MB_FUNC_READ_COILS)
                {
                    uint8_t coils[(p_read.length + 7) / 8];
                    nmbs_bitfield_reset(coils);
                    nmbs_error err = nmbs_read_coils(&nmbs, p_read.address, p_read.length, coils);
                    // TODO: Store response in the memory map
                }
                else if (p_read.function == MB_FUNC_READ_HOLDING_REGISTERS)
                {
                    uint16_t registers[p_read.length];
                    nmbs_error err = nmbs_read_holding_registers(&nmbs, p_read.address, p_read.length, registers);
                    // TODO: Store response in the memory map
                }
            }
        }

        // Handle arbitrary read/write
        // TODO: When we receive a m_read/m_write message, dispatch a message in the modbus bus and on response, send the message back to the host

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void bus_init(struct bus_context *bus_context)
{
    BaseType_t result = xTaskCreate(bus_task,
                                    name[bus_context->bus],
                                    configMINIMAL_STACK_SIZE,
                                    bus_context,
                                    tskDEFAULT_PRIORITY,
                                    &taskHandlers[bus_context->bus]);

    if (result != pdPASS)
    {
        printf("Bus Task creation failed!");
    }
}