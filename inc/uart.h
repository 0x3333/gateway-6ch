#ifndef UART_H_
#define UART_H_

#include <FreeRTOS.h>
#include "stream_buffer.h"

#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/uart.h"

//
// Defines
//

#ifndef PIO_RX_BUFFER_SIZE
#define PIO_RX_BUFFER_SIZE 64
#endif

#ifndef PIO_TX_BUFFER_SIZE
#define PIO_TX_BUFFER_SIZE 64
#endif

#define MAX_PIO_UARTS ((size_t)(NUM_PIOS * NUM_PIO_STATE_MACHINES / 2))

//
// Structures
//

struct uart
{
    uart_inst_t *const uart;

    const uint32_t baudrate;

    const uint rx_pin;
    const uint tx_pin;
};

struct pio_uart
{
    const struct uart super;
    const uint en_pin;

    PIO rx_pio;
    uint rx_sm;
    StreamBufferHandle_t rx_sbuffer;

    PIO tx_pio;
    uint tx_sm;
    int tx_dma_channel;
    dma_channel_config tx_dma_config;
    volatile bool is_transmitting;
    uint8_t tx_dma_buffer_length;
    uint32_t tx_dma_buffer[PIO_TX_BUFFER_SIZE];
};

// NULL terminated array with enabled PIO UARTS
extern struct pio_uart *pio_uarts[MAX_PIO_UARTS + 1];

//
// ESP <> PICO UART
//

extern const struct uart esp_uart;

//
// RS485 UARTs
//

extern struct pio_uart pio_uart_bus_1; // Assigned to ESP
extern struct pio_uart pio_uart_bus_2; // Assigned to ESP
extern struct pio_uart pio_uart_bus_3;
extern struct pio_uart pio_uart_bus_4;
extern struct pio_uart pio_uart_bus_5;
extern struct pio_uart pio_uart_bus_6;

//
// Functions
//

/**
 * Initialize all UART `uart` structs.
 */
void init_uarts(int count, ...);

/**
 * Initialize all PIO UART `pio_uart` structs.
 */
void init_pio_uart(struct pio_uart *const pio_uart);

/**
 * Start a TX DMA transfer.
 */
void pio_tx_dma_start_transfer(struct pio_uart *uart, size_t transfer_size);

#endif // UART_H_