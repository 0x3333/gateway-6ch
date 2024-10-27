#include <stdarg.h>

#include "debug.h"
#include "uart.h"
#include "time.h"

/*
 * PIO UART
 *
 * We have 2 PIO state machines per UART channel.
 * By default, RX channel is located in PIO0, TX channel in PIO1.
 * Can be changed using PIO_UART_RX_PIO and PIO_UART_TX_PIO defines
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

// Prototypes
static void init_pio_irq(struct pio_uart *uart);
static void init_pio_dma(struct pio_uart *uart);
static void pio_uart_rx_irq_handler(void);
static void pio_uart_tx_irq_handler(void);

//
// ESP <> PICO UART

const struct uart esp_uart = {
    .baudrate = 115200,
    .rx_pin = 5,
    .tx_pin = 4,
    .uart = uart1,
};

//
// RS485 UARTs

struct pio_uart pio_uart_bus_1 = {
    .super = {
        .baudrate = 115200,
        .rx_pin = 7,
        .tx_pin = 8,
        .uart = NULL,
    },
    .en_pin = 9,
};

struct pio_uart pio_uart_bus_2 = {
    .super = {
        .baudrate = 115200,
        .rx_pin = 10,
        .tx_pin = 11,
        .uart = NULL,
    },
    .en_pin = 12,
};

struct pio_uart pio_uart_bus_3 = {
    .super = {
        .baudrate = 115200,
        .rx_pin = 13,
        .tx_pin = 14,
        .uart = NULL,
    },
    .en_pin = 15,
};

struct pio_uart pio_uart_bus_4 = {
    .super = {
        .baudrate = 115200,
        .rx_pin = 18,
        .tx_pin = 16,
        .uart = NULL,
    },
    .en_pin = 17,
};

struct pio_uart pio_uart_bus_5 = {
    .super = {
        .baudrate = 115200,
        .rx_pin = 19,
        .tx_pin = 20,
        .uart = NULL,
    },
    .en_pin = 21,
};

struct pio_uart pio_uart_bus_6 = {
    .super = {
        .baudrate = 115200,
        .rx_pin = 28,
        .tx_pin = 26,
        .uart = NULL,
    },
    .en_pin = 27,
};

struct pio_uart *pio_uarts[MAX_PIO_UARTS + 1] = {NULL};

//
// Init Functions

void init_uarts(int count, ...)
{
    va_list va;
    va_start(va, count);
    for (int i = 0; i < count; i++)
    {
        struct uart *uart = va_arg(va, struct uart *);
        uart_init(uart->uart, uart->baudrate);
        gpio_set_function(uart->tx_pin, GPIO_FUNC_UART);
        gpio_set_function(uart->rx_pin, GPIO_FUNC_UART);
    }
    va_end(va);
}

//
// Init PIO UARTs

void init_pio_uart(struct pio_uart *const pio_uart)
{
    static uint rx_offset = 0;
    static uint tx_offset = 0;

    for (size_t i = 0; pio_uarts[i] != NULL; i++)
    {
        if (pio_uarts[i] == pio_uart)
        {
            panic("PIO UART on RX Pin %u already configured!", pio_uart->super.rx_pin);
        }
    }

    if (!rx_offset)
    {
        rx_offset = pio_add_program(PIO_UART_RX_PIO, &uart_rx_program);
    }
    if (!tx_offset)
    {
        tx_offset = pio_add_program(PIO_UART_TX_PIO, &uart_tx_program);
    }

    // We know that there is free SMs, so no checks

    pio_uart->rx_pio = PIO_UART_RX_PIO;
    pio_uart->rx_sm = pio_claim_unused_sm(pio_uart->rx_pio, true);
    uart_rx_program_init(pio_uart->rx_pio, pio_uart->rx_sm, rx_offset, pio_uart->super.rx_pin, pio_uart->super.baudrate);

    pio_uart->tx_pio = PIO_UART_TX_PIO;
    pio_uart->tx_sm = pio_claim_unused_sm(pio_uart->tx_pio, true);
    uart_tx_program_init(pio_uart->tx_pio, pio_uart->tx_sm, tx_offset, pio_uart->super.tx_pin, pio_uart->en_pin, pio_uart->super.baudrate);

    pio_uart->rx_sbuffer = xStreamBufferCreate(PIO_RX_BUFFER_SIZE, 1);
    pio_uart->is_transmitting = false;

    init_pio_irq(pio_uart);
    init_pio_dma(pio_uart);

    size_t i = 0;
    while (pio_uarts[i] != NULL)
    {
        i++;
    }
    if (i < MAX_PIO_UARTS)
    {
        pio_uarts[i] = pio_uart;
    }
}

//
// Init PIO RX and TX IRQs
static void init_pio_irq(struct pio_uart *const uart)
{
    enum irq_num_rp2040 rx_irq = PIO_UART_RX_PIO == pio0 ? PIO0_IRQ_0 : PIO1_IRQ_0;
    enum irq_num_rp2040 tx_irq = PIO_UART_TX_PIO == pio0 ? PIO0_IRQ_0 : PIO1_IRQ_0;

    if (!irq_get_exclusive_handler(rx_irq))
    {
        irq_set_exclusive_handler(rx_irq, pio_uart_rx_irq_handler);
        irq_set_enabled(rx_irq, true);
    }

    if (!irq_get_exclusive_handler(tx_irq))
    {
        irq_set_exclusive_handler(tx_irq, pio_uart_tx_irq_handler);
        irq_set_enabled(tx_irq, true);
    }

    pio_set_irqn_source_enabled(uart->rx_pio, 0, pis_sm0_rx_fifo_not_empty + uart->rx_sm, true);
    pio_set_irqn_source_enabled(uart->tx_pio, 0, pis_interrupt0 + uart->tx_sm, true);
}

//
// Init PIO TX DMA
static void init_pio_dma(struct pio_uart *uart)
{
    uart->tx_dma_channel = dma_claim_unused_channel(true);
    uart->tx_dma_config = dma_channel_get_default_config(uart->tx_dma_channel);
    channel_config_set_transfer_data_size(&uart->tx_dma_config, DMA_SIZE_32);
    channel_config_set_dreq(&uart->tx_dma_config, pio_get_dreq(uart->tx_pio, uart->tx_sm, true));
}

void pio_tx_dma_start_transfer(struct pio_uart *uart, size_t transfer_size)
{
    if (uart->is_transmitting)
    {
        panic("PIO UART(RX: %u) is already transmitting!", uart->super.rx_pin);
    }

    uart->is_transmitting = true;
    dma_channel_configure(
        uart->tx_dma_channel,            // DMA channel number
        &uart->tx_dma_config,            // Channel configuration
        &uart->tx_pio->txf[uart->tx_sm], // Destination: PIO TX FIFO
        uart->tx_dma_buffer,             // Source: data buffer
        transfer_size,                   // Number of transfers
        true                             // Start immediately
    );
}

//  PIO RX IRQ Handler
static void pio_uart_rx_irq_handler(void)
{
    for (size_t i = 0; pio_uarts[i] != NULL; i++)
    {
        while (!pio_rx_empty(pio_uarts[i]))
        {
            uint8_t c = pio_rx_getc(pio_uarts[i]);
            if (!xStreamBufferSendFromISR(pio_uarts[i]->rx_sbuffer, &c, sizeof(c), NULL))
            {
                // FIXME: Convert all Panic to some kind fo log in flash, so the watchdog could recovery the application, but the log will be stored.
                panic("PIO UART RX(%zu): Buffer is full", i);
            }
        }
    }
}

//  PIO TX IRQ Handler
static void pio_uart_tx_irq_handler(void)
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
