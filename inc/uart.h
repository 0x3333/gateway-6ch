#ifndef UART_H_
#define UART_H_

#include <stdint.h>
#include <FreeRTOS.h>
#include "stream_buffer.h"

#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/uart.h"

//
// Defines
//

#if PICO_RP2040
#define PIO_FIFO_DEEP 4
#define UART_FIFO_DEEP 32
#else
#error "CPU Not supported!"
#endif

// We want all buffer to have 64 bytes in total length, including the native FIFOs.
#ifndef TOTAL_BUFFER_SIZE
#define TOTAL_BUFFER_SIZE 64
#endif

#ifndef UART_TX_BUFFER_SIZE
#define UART_TX_BUFFER_SIZE (TOTAL_BUFFER_SIZE - UART_FIFO_DEEP)
#endif

#ifndef PIO_BUFFER_SIZE
// We merge the PIO FIFO, as each state machine only has one direction
#define PIO_BUFFER_SIZE (TOTAL_BUFFER_SIZE - (PIO_FIFO_DEEP * 2))
#endif

// Max number of PIO UARTs possible
#define NUM_PIO_UARTS ((size_t)(NUM_PIOS * NUM_PIO_STATE_MACHINES / 2))

//
// Structures
//

struct uart
{
    const uint32_t baudrate;

    const uint rx_pin;
    const uint tx_pin;
};

struct hw_uart
{
    const struct uart super;
    uart_inst_t *const uart;

    StreamBufferHandle_t tx_sbuffer;
};

struct pio_uart
{
    const struct uart super;
    const uint en_pin;

    PIO rx_pio;
    uint rx_sm;
    StreamBufferHandle_t rx_sbuffer;
    bool rx_sbuffer_overrun;

    PIO tx_pio;
    uint tx_sm;
    int tx_dma_channel;
    dma_channel_config tx_dma_config;
    volatile bool is_transmitting;
    uint8_t tx_dma_buffer_length;
    uint8_t tx_dma_buffer[PIO_BUFFER_SIZE];
};

//
// Variables
//

// NULL Sentinel-terminated array with enabled Hardware UARTS
extern struct hw_uart *hw_uarts[NUM_UARTS + 1];
// NULL Sentinel-terminated array with enabled PIO UARTS
extern struct pio_uart *pio_uarts[NUM_PIO_UARTS + 1];

// ESP <> PICO Hardware UART
extern struct hw_uart esp_uart;

//
// RS485 UARTs

extern struct pio_uart pio_uart_bus_1; // In default board, assigned to ESP
extern struct pio_uart pio_uart_bus_2; // In default board, assigned to ESP
extern struct pio_uart pio_uart_bus_3;
extern struct pio_uart pio_uart_bus_4;
extern struct pio_uart pio_uart_bus_5;
extern struct pio_uart pio_uart_bus_6;

//
// Functions
//

/**
 * Initialize a Hardware UART.
 */
void init_hw_uart(struct hw_uart *const uart);

/**
 * Initialize a PIO UART.
 */
void init_pio_uart(struct pio_uart *const pio_uart);

/**
 * Write to a PIO UART using DMA.
 * The data bytes are copied into the DMA buffer.
 * Multiple calls to this function will block until the DMA transfer has terminated.
 */
void pio_uart_write(struct pio_uart *const uart, const uint8_t *data, uint8_t length);

/**
 * Read bytes from a PIO UART.
 * @return Number of bytes read. May not be equal to data_length.
 */
uint8_t pio_uart_read(struct pio_uart *const uart, uint8_t *data, uint8_t length);

/**
 * Returns if a PIO UART is writable, otherwise, it is transmitting, blocking the DMA buffer
 */
bool pio_uart_writable(struct pio_uart *const uart);

/**
 * Write data to a Hardware UART.
 * The data bytes are copied into the Stream buffer.
 */
void hw_uart_write(struct hw_uart *const hw_uart, const uint8_t *data, uint8_t length);

/**
 * Read bytes from a Hardware UART.
 * @return Number of bytes read. May not be equal to data_length.
 */
uint8_t hw_uart_read(struct hw_uart *const uart, uint8_t *data, uint8_t length);

#endif // UART_H_