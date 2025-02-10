#ifndef UART_H_
#define UART_H_

#include <stdint.h>
#include "ring_buffer.h"

#include "hardware/pio.h"
#include "hardware/uart.h"

//
// Defines
//

#if PICO_RP2040
#define PIO_FIFO_DEEP 4
#define HW_UART_FIFO_DEEP 32
#else
#error "CPU Not supported!"
#endif

#ifndef UARTS_BUFFER_SIZE
#define UARTS_BUFFER_SIZE 64
#endif

// Max number of PIO UARTs possible
#define COUNT_HW_UARTS NUM_UARTS
#define COUNT_PIO_UARTS 6u
#define MAX_PIO_UARTS (NUM_PIOS * NUM_PIO_STATE_MACHINES / 2u)

/*
 * PIO UART
 *
 * We use 2 PIO state machines per UART channel.
 * By default, RX channel is located in PIO0(PIO_UART_RX_PIO), TX channel in PIO1(PIO_UART_TX_PIO).
 */

#ifndef PIO_UART_RX_PIO
#define PIO_UART_RX_PIO pio0
#endif

#ifndef PIO_UART_TX_PIO
#define PIO_UART_TX_PIO pio1
#endif

#ifndef PIO_UART_RX_FIFO_IRQ_INDEX
#define PIO_UART_RX_FIFO_IRQ_INDEX 0
#endif

#ifndef PIO_UART_TX_DONE_IRQ_INDEX
#define PIO_UART_TX_DONE_IRQ_INDEX 0
#endif

#ifndef PIO_UART_TX_FIFO_IRQ_INDEX
#define PIO_UART_TX_FIFO_IRQ_INDEX 1
#endif

#if PIO_UART_TX_DONE_IRQ_INDEX == PIO_UART_TX_FIFO_IRQ_INDEX
#error PIO_UART_TX_DONE_IRQ_INDEX cannot be equal PIO_UART_TX_FIFO_IRQ_INDEX
#endif

#ifndef HW_UART_DEFAULT_BAUDRATE
#define HW_UART_DEFAULT_BAUDRATE 230400
#endif

#ifndef PIO_UART_DEFAULT_BAUDRATE
#define PIO_UART_DEFAULT_BAUDRATE 115200
#endif

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

    ring_buffer_t tx_rbuffer;         // TX Ring Buffer
    volatile bool tx_rbuffer_overrun; // If the TX Ring Buffer has overrun
    ring_buffer_t rx_rbuffer;         // RX Ring Buffer
    volatile bool rx_rbuffer_overrun; // If the RX Ring Buffer has overrun
    const uint32_t id;                // Used to identify this UART between all instances

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

extern struct hw_uart *active_hw_uarts[COUNT_HW_UARTS];
extern struct pio_uart *active_pio_uarts[COUNT_PIO_UARTS];

//
// Hardware UARTs
// Note: UART0 is not available in our board(No pin available)

extern struct hw_uart hw_uart1;

//
// PIO UARTs

extern struct pio_uart pio_uart_0; // In the default configuration, assigned to HOST
extern struct pio_uart pio_uart_1; // In the default configuration, assigned to HOST
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
void hw_uart_flush_rx(struct hw_uart *const uart);

/**
 * Flush the RX of a PIO UART.
 */
void pio_uart_flush_rx(struct pio_uart *const uart);

#endif // UART_H_