#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "uart.h"
#include <task.h>

#include "debug.h"
#include "time.h"

// FIXME: Convert all Panic to some kind fo log in flash, so the watchdog could recovery the application, but the log will be stored.

/*
 * PIO UART
 *
 * We use 2 PIO state machines per UART channel.
 * By default, RX channel is located in PIO0(PIO_UART_RX_PIO), TX channel in PIO1(PIO_UART_TX_PIO).
 * TX channel is feed by DMA, RX channel is read to a FreeRTOS StreamBuffer.
 */

#ifndef PIO_UART_RX_PIO
#define PIO_UART_RX_PIO pio0
#endif

#ifndef PIO_UART_TX_PIO
#define PIO_UART_TX_PIO pio1
#endif

// PIO Programs
#include "uart_tx.pio.h"
#include "uart_rx.pio.h"

//
// Prototypes
static void isr_pio_uart_rx(void);
static void isr_pio_uart_tx(void);
static void isr_hw_uart_tx(void);

//
// ESP <> PICO UART

struct hw_uart esp_uart = {
    .super = {
        .baudrate = 230400,
        .rx_pin = 5,
        .tx_pin = 4,
    },
    .uart = uart1,
};

//
// RS485 UARTs

struct pio_uart pio_uart_bus_1 = {
    .super = {
        .baudrate = 115200,
        .rx_pin = 7,
        .tx_pin = 8,
    },
    .id = 1,
    .en_pin = 9,
};

struct pio_uart pio_uart_bus_2 = {
    .super = {
        .baudrate = 115200,
        .rx_pin = 10,
        .tx_pin = 11,
    },
    .id = 2,
    .en_pin = 12,
};

struct pio_uart pio_uart_bus_3 = {
    .super = {
        .baudrate = 115200,
        .rx_pin = 13,
        .tx_pin = 14,
    },
    .id = 3,
    .en_pin = 15,
};

struct pio_uart pio_uart_bus_4 = {
    .super = {
        .baudrate = 115200,
        .rx_pin = 18,
        .tx_pin = 16,
    },
    .id = 4,
    .en_pin = 17,
};

struct pio_uart pio_uart_bus_5 = {
    .super = {
        .baudrate = 115200,
        .rx_pin = 19,
        .tx_pin = 20,
    },
    .id = 5,
    .en_pin = 21,
};

struct pio_uart pio_uart_bus_6 = {
    .super = {
        .baudrate = 115200,
        .rx_pin = 28,
        .tx_pin = 26,
    },
    .id = 6,
    .en_pin = 27,
};

struct hw_uart *hw_uarts[NUM_UARTS + 1] = {NULL};
struct pio_uart *pio_uarts[NUM_PIO_UARTS + 1] = {NULL};

//
// Hardware UARTs
//

void init_hw_uart(struct hw_uart *const hw_uart)
{
    // Save Hardware UART to a sentinel-terminated array
    size_t i = 0;
    while (hw_uarts[i] != NULL)
    {
        i++;
    }
    if (i == NUM_UARTS)
    {
        panic("No more space for a Hardware UART!");
    }
    hw_uarts[i] = hw_uart;

    // Initialize Hardware UART
    uart_init(hw_uart->uart, hw_uart->super.baudrate);
    gpio_set_function(hw_uart->super.tx_pin, GPIO_FUNC_UART);
    gpio_set_function(hw_uart->super.rx_pin, GPIO_FUNC_UART);
    gpio_set_pulls(hw_uart->super.tx_pin, true, false);
    gpio_set_pulls(hw_uart->super.rx_pin, true, false);
    uart_set_hw_flow(hw_uart->uart, false, false);
    uart_set_format(hw_uart->uart, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(hw_uart->uart, true);
    hw_uart->tx_sbuffer = xStreamBufferCreate(UART_TX_BUFFER_SIZE, 1);

    // Enable IRQ but keep it off until the Queue is filled
    irq_set_exclusive_handler(UART_IRQ_NUM(hw_uart->uart), isr_hw_uart_tx);
    irq_set_enabled(UART_IRQ_NUM(hw_uart->uart), true);
    uart_set_irqs_enabled(hw_uart->uart, false, true);
}

//
// Hardware UART Write
void hw_uart_write(struct hw_uart *const uart, const uint8_t *data, uint8_t length)
{
    // First, fill the UART FIFO...
    while (uart_is_writable(uart->uart) && length > 0)
    {
        uart_putc_raw(uart->uart, *data++);
        length--;
    }
    // ...then, we fill the exceeding data to the StreamBuffer
    if (length > 0)
    {
        size_t written = xStreamBufferSend(uart->tx_sbuffer, &data, length, 0);
        assert(written == length);

        uart_set_irqs_enabled(uart->uart, false, true);
    }
}

//
// Hardware UART Read
uint8_t hw_uart_read(struct hw_uart *const uart, uint8_t *data, uint8_t length)
{
    uint8_t count = 0;
    while (uart_is_readable(uart->uart) && count < length)
    {
        // We read directly from the register, avoid a double uart_is_readable
        *data++ = (uint8_t)uart_get_hw(uart->uart)->dr;
        count++;
    }
    return count;
}

static void isr_hw_uart_tx(void)
{
    static uint8_t data;

    for (size_t i = 0; hw_uarts[i] != NULL; i++)
    {
        // Check if there is data in the StreamBuffer and UART has space in FIFO
        size_t length = xStreamBufferReceiveFromISR(hw_uarts[i]->tx_sbuffer, &data, 1, NULL);
        while (length && uart_is_writable(hw_uarts[i]->uart))
        {
            // Fill the FIFO with data
            uart_putc_raw(hw_uarts[i]->uart, data);
        }
        // if the StreamBuffer is empty, disable IRQ, no data to send
        if (xStreamBufferIsEmpty(hw_uarts[i]->tx_sbuffer))
        {
            uart_set_irqs_enabled(hw_uarts[i]->uart, false, false);
        }
    }
}

//
// PIO UARTs
//

//
// Init PIO UARTs
void init_pio_uart(struct pio_uart *const pio_uart)
{
    static int rx_program_offset = PICO_ERROR_GENERIC;
    static int tx_program_offset = PICO_ERROR_GENERIC;

    static const enum irq_num_rp2040 rx_irq = PIO_IRQ_NUM(PIO_UART_RX_PIO, 0); // All RX use the same IRQ
    static const enum irq_num_rp2040 tx_irq = PIO_IRQ_NUM(PIO_UART_TX_PIO, 0); // All TX use the same IRQ

    // Save PIO UART to a sentinel-terminated array
    size_t i = 0;
    while (pio_uarts[i] != NULL)
    {
        i++;
    }
    if (i == NUM_PIO_UARTS)
    {
        panic("No more space for a Hardware UART!");
    }

    // Check if this PIO UART has already been initialized
    for (size_t i = 0; pio_uarts[i] != NULL; i++)
    {
        if (pio_uarts[i] == pio_uart)
        {
            // Issue a warning and ignore already initialized PIO UARTs
            printf("[WARN] PIO UART already initialized.\n");
            return;
        }
    }
    pio_uarts[i] = pio_uart; // Store it in the array

    // Add program if not already
    if (rx_program_offset <= PICO_ERROR_GENERIC)
    {
        rx_program_offset = pio_add_program(PIO_UART_RX_PIO, &uart_rx_program);
    }
    if (tx_program_offset <= PICO_ERROR_GENERIC)
    {
        tx_program_offset = pio_add_program(PIO_UART_TX_PIO, &uart_tx_program);
    }

    // Initialize RX PIO and State Machine
    pio_uart->rx_pio = PIO_UART_RX_PIO;
    int sm = pio_claim_unused_sm(pio_uart->rx_pio, true);
    if (sm == -1)
    {
        panic("No RX State Machine available!");
    }
    pio_uart->rx_sm = sm;
    uart_rx_program_init(pio_uart->rx_pio, pio_uart->rx_sm, (uint)rx_program_offset, pio_uart->super.rx_pin, pio_uart->super.baudrate);
    // Initialize TX PIO and State Machine
    pio_uart->tx_pio = PIO_UART_TX_PIO;
    sm = pio_claim_unused_sm(pio_uart->tx_pio, true);
    if (sm == -1)
    {
        panic("No TX State Machine available!");
    }
    pio_uart->tx_sm = sm;
    uart_tx_program_init(pio_uart->tx_pio, pio_uart->tx_sm, (uint)tx_program_offset, pio_uart->super.tx_pin, pio_uart->en_pin, pio_uart->super.baudrate);

    // Initialize RX StreamBuffer
    pio_uart->rx_sbuffer = xStreamBufferCreate(PIO_BUFFER_SIZE, 1);
    pio_uart->rx_sbuffer_overrun = false;

    // Initialize RX IRQ
    if (!irq_get_exclusive_handler(rx_irq))
    {
        irq_set_exclusive_handler(rx_irq, isr_pio_uart_rx);
        irq_set_enabled(rx_irq, true);
    }
    pio_set_irqn_source_enabled(pio_uart->rx_pio, 0, pio_get_rx_fifo_not_empty_interrupt_source(pio_uart->rx_sm), true);
    // Initialize TX IRQ
    if (!irq_get_exclusive_handler(tx_irq))
    {
        irq_set_exclusive_handler(tx_irq, isr_pio_uart_tx);
        irq_set_enabled(tx_irq, true);
    }
    pio_set_irqn_source_enabled(pio_uart->tx_pio, 0, pis_interrupt0 + pio_uart->tx_sm, true);

    // Initialize TX DMA
    int dma_channel = dma_claim_unused_channel(true);
    if (pio_uart->tx_dma_channel <= PICO_ERROR_GENERIC)
    {
        panic("No TX DMA Channel available!");
    }
    pio_uart->tx_dma_channel = dma_channel;
    pio_uart->tx_dma_config = dma_channel_get_default_config(pio_uart->tx_dma_channel);
    channel_config_set_transfer_data_size(&pio_uart->tx_dma_config, DMA_SIZE_8);
    channel_config_set_dreq(&pio_uart->tx_dma_config, pio_get_dreq(pio_uart->tx_pio, pio_uart->tx_sm, true));
}

//
// PIO UART is writable
bool pio_uart_writable(struct pio_uart *const uart)
{
    return !uart->is_transmitting;
}

//
// PIO UART Write
// FIXME: Create another way of writing to the UART, using the same method that we use in the hardware uart
void pio_uart_write(struct pio_uart *const uart, const uint8_t *data, uint8_t length)
{
    if (length > PIO_BUFFER_SIZE)
    {
        printf("[WARN] PIO UART write is bigger than DMA Buffer(%u), discarding excess.\n", PIO_BUFFER_SIZE);
    }

    // Block task if DMA is still transmitting
    bool issued = false;
    while (uart->is_transmitting)
    {
        if (!issued)
        {
            issued = true;
            printf("[WARN] pio_uart_write delaying.");
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    uart->is_transmitting = true;

    // Transfer data array to DMA buffer
    memcpy(uart->tx_dma_buffer, data, MIN(PIO_BUFFER_SIZE, length));

    // Start DMA transfer
    dma_channel_configure(
        uart->tx_dma_channel,            // DMA channel number
        &uart->tx_dma_config,            // Channel configuration
        &uart->tx_pio->txf[uart->tx_sm], // Destination: PIO TX FIFO
        uart->tx_dma_buffer,             // Source: data buffer
        length,                          // Number of transfers
        true                             // Start immediately
    );
}

//
// PIO UART Read
inline uint8_t pio_uart_read(struct pio_uart *const uart, uint8_t *data, uint8_t length)
{
    return xStreamBufferReceive(uart->rx_sbuffer, data, length, 0);
}

//
//  PIO RX IRQ Handler
static void isr_pio_uart_rx(void)
{
    for (size_t i = 0; pio_uarts[i] != NULL; i++)
    {
        while (!pio_rx_empty(pio_uarts[i]))
        {
            uint8_t c = pio_rx_getc(pio_uarts[i]);
            if (!xStreamBufferSendFromISR(pio_uarts[i]->rx_sbuffer, &c, sizeof(c), NULL))
            {
                pio_uarts[i]->rx_sbuffer_overrun = true;
            }
        }
    }
}

//
//  PIO TX IRQ Handler
static void isr_pio_uart_tx(void)
{
    for (size_t i = 0; pio_uarts[i] != NULL; i++)
    {
        // Check which SM issued the IRQ
        if (pio_interrupt_get(pio_uarts[i]->tx_pio, pio_uarts[i]->tx_sm))
        {
            // Clear the flag and set as not transmitting
            pio_interrupt_clear(pio_uarts[i]->tx_pio, pio_uarts[i]->tx_sm);
            pio_uarts[i]->is_transmitting = false;
        }
    }
}

//
// Task Maintenance

static void task_uart_maintenance(void *arg)
{
    (void)arg;

    while (true)
    {
        for (size_t i = 0; pio_uarts[i] != NULL; i++)
        {
            // Check for overrun on RX StringBuffer
            if (pio_uarts[i]->rx_sbuffer_overrun)
            {
                pio_uarts[i]->rx_sbuffer_overrun = false;
                printf("[WARN] PIO UART %u RX Overrun.", pio_uarts[i]->id);
            }
        }

        // TODO: Implement LED blinking when TX/RX

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void init_uart_maintenance_task(void)
{
    xTaskCreate(task_uart_maintenance, "UART-M", configMINIMAL_STACK_SIZE, NULL, tskLOW_PRIORITY, NULL);
}
