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
#define HW_UART_FIFO_DEEP 32
#else
#error "CPU Not supported!"
#endif

#ifndef UARTS_BUFFER_SIZE
#define UARTS_BUFFER_SIZE 64
#endif

// Max number of PIO UARTs possible
#define NUM_PIO_UARTS ((size_t)(NUM_PIOS * NUM_PIO_STATE_MACHINES / 2))

#define UART_TYPE(id) (id <= 3 ? "HW" : "PIO")
#define UART_PIO_ID(id) (id << 2)
#define UART_ID(id) (id <= 3 ? id : id >> 2)

//
// Structures
//

/**
 * Hardware UART ID: 0 ~ 2
 * PIO UART ID: 1 ~ 6 (shifted 2 bits left)
 */
struct uart
{
    const uint32_t baudrate;

    const uint rx_pin;
    const uint tx_pin;

    StreamBufferHandle_t tx_sbuffer;  // TX Stream Buffer
    volatile bool tx_sbuffer_overrun; // If the TX Stream Buffer has overrun
    StreamBufferHandle_t rx_sbuffer;  // RX Stream Buffer
    volatile bool rx_sbuffer_overrun; // If the RX Stream Buffer has overrun
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

// NULL Sentinel-terminated array with enabled Hardware UARTS
extern struct hw_uart *hw_uarts[NUM_UARTS + 1];
// NULL Sentinel-terminated array with enabled PIO UARTS
extern struct pio_uart *pio_uarts[NUM_PIO_UARTS + 1];

// ESP <> PICO Hardware UART
extern struct hw_uart esp_uart;

//
// RS485 UARTs

extern struct pio_uart pio_uart_bus_1; // In the default board, assigned to ESP
extern struct pio_uart pio_uart_bus_2; // In the default board, assigned to ESP
extern struct pio_uart pio_uart_bus_3;
extern struct pio_uart pio_uart_bus_4;
extern struct pio_uart pio_uart_bus_5;
extern struct pio_uart pio_uart_bus_6;
extern volatile bool uart_activity;

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
 * Initialize the UART Maintenance Task
 */
void init_uart_maintenance_task(void);

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
 * Read bytes from a Hardware UART.
 * @return Number of bytes read. May not be equal to data_length.
 */
size_t hw_uart_read_bytes(struct hw_uart *const uart, void *dst, uint8_t size);

/**
 * Read bytes from a PIO UART.
 * @return Number of bytes read. May not be equal to data_length.
 */
size_t pio_uart_read_bytes(struct pio_uart *const uart, void *dst, uint8_t size);

#endif // UART_H_