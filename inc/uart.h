#ifndef UART_H_
#define UART_H_

#include <stdint.h>
#include <FreeRTOS.h>
#include <stream_buffer.h>

#include "hardware/pio.h"
#include "hardware/uart.h"

#include "config.h"

//
// Data Structures
//

/**
 * Hardware UART ID: 0 ~ 2
 * PIO UART ID: 0 ~ 5
 */
struct uart
{
    const char *type;
    uint32_t baudrate;

    const uint rx_pin;
    const uint tx_pin;

    StreamBufferHandle_t tx_buffer;  // TX Buffer
    volatile bool tx_buffer_overrun; // If the TX Buffer has overrun
    StreamBufferHandle_t rx_buffer;  // RX Buffer
    volatile bool rx_buffer_overrun; // If the RX Buffer has overrun
    const uint32_t id;               // Used to identify this UART between all instances

    volatile bool activity; // If there is activity in this UART
};

struct hw_uart
{
    struct uart super;
    uart_inst_t *const native_uart;
};

struct pio_uart
{
    struct uart super;
    const uint en_pin;

    PIO rx_pio;
    uint rx_sm;

    PIO tx_pio;
    uint tx_sm;
    volatile bool tx_done;
};

//
// Variables
//

// Null terminated array of Hardware UARTs
extern struct hw_uart *active_hw_uarts[COUNT_HW_UARTS + 1];
// Null terminated array of PIO UARTs
extern struct pio_uart *active_pio_uarts[COUNT_PIO_UARTS + 1];

//
// Hardware UARTs
// Note: UART0 is not available in our board(No pin available)

extern struct hw_uart hw_uart1;

//
// PIO UARTs

extern struct pio_uart pio_uart_0;
extern struct pio_uart pio_uart_1;
extern struct pio_uart pio_uart_2;
extern struct pio_uart pio_uart_3;
extern struct pio_uart pio_uart_4;
extern struct pio_uart pio_uart_5;

// Used to inform the LED task that has been some activity in some UART.
extern volatile bool uart_activity;

//
// Prototypes
//

/**
 * Initialize a Hardware UART.
 */
void hw_uart_init(struct hw_uart *const uart);

/**
 * Initialize a PIO UART.
 */
void pio_uart_init(struct pio_uart *const pio_uart);

/**
 * Return a PIO UART by its index.
 */
struct pio_uart *get_pio_uart_by_index(uint8_t index);

/**
 * Initialize the UART Maintenance Task.
 */
void uart_maintenance_init(void);

/**
 * Write data to a Hardware UART.
 * The data bytes are copied into the Stream buffer.
 */
size_t hw_uart_write_bytes(struct hw_uart *const uart, const void *src, size_t size);

/**
 * Write data to a PIO UART.
 * The data bytes are copied into the Stream buffer.
 */
size_t pio_uart_write_bytes(struct pio_uart *const uart, const void *src, size_t size);

/**
 * Read a byte from a Hardware UART.
 * @return If a byte has been read or not.
 */
bool hw_uart_read_byte(struct hw_uart *const uart, void *dst);

/**
 * Read bytes from a Hardware UART.
 * @return Number of bytes read. May not be equal to data_length.
 */
size_t hw_uart_read_bytes(struct hw_uart *const uart, void *dst, uint8_t size);

/**
 * Read a byte from a PIO UART.
 * @return If a byte has been read or not.
 */
bool pio_uart_read_byte(struct pio_uart *const uart, void *dst);

/**
 * Read bytes from a PIO UART.
 * @return Number of bytes read. May not be equal to data_length.
 */
size_t pio_uart_read_bytes(struct pio_uart *const uart, void *dst, uint8_t size);

/**
 * Flush the RX of a Hardware UART.
 */
void hw_uart_rx_flush(struct hw_uart *const uart);

/**
 * Flush the RX of a PIO UART.
 */
void pio_uart_rx_flush(struct pio_uart *const uart);

/**
 * Return the remaining space in the TX buffer of a Hardware UART.
 */
size_t hw_uart_tx_buffer_remaining(const struct hw_uart *uart);

/**
 * Return the remaining space in the TX buffer of a PIO UART.
 */
size_t pio_uart_tx_buffer_remaining(const struct pio_uart *uart);

#endif // UART_H_