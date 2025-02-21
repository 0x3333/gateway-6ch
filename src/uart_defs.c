#include "uart.h"
//
// Hardware UARTs

struct hw_uart hw_uart1 = {
    .super = {
        .type = "HW",
        .baudrate = HW_UART_DEFAULT_BAUDRATE,
        .rx_pin = 5,
        .tx_pin = 4,
        .id = 1,
    },
    .native_uart = uart1,
};

//
// RS485 UARTs

struct pio_uart pio_uart_0 = {
    .super = {
        .type = "PIO",
        .baudrate = PIO_UART_DEFAULT_BAUDRATE,
        .rx_pin = 7,
        .tx_pin = 8,
        .id = 0,
    },
    .en_pin = 9,
};

struct pio_uart pio_uart_1 = {
    .super = {
        .type = "PIO",
        .baudrate = PIO_UART_DEFAULT_BAUDRATE,
        .rx_pin = 10,
        .tx_pin = 11,
        .id = 1,
    },
    .en_pin = 12,
};

struct pio_uart pio_uart_2 = {
    .super = {
        .type = "PIO",
        .baudrate = PIO_UART_DEFAULT_BAUDRATE,
        .rx_pin = 13,
        .tx_pin = 14,
        .id = 2,
    },
    .en_pin = 15,
};

struct pio_uart pio_uart_3 = {
    .super = {
        .type = "PIO",
        .baudrate = PIO_UART_DEFAULT_BAUDRATE,
        .rx_pin = 18,
        .tx_pin = 16,
        .id = 3,
    },
    .en_pin = 17,
};

struct pio_uart pio_uart_4 = {
    .super = {
        .type = "PIO",
        .baudrate = PIO_UART_DEFAULT_BAUDRATE,
        .rx_pin = 19,
        .tx_pin = 20,
        .id = 4,
    },
    .en_pin = 21,
};

//
// Note: Bus 6 is now associated with DMX
//
// struct pio_uart pio_uart_5 = {
//     .super = {
//         .type = "PIO",
//         .baudrate = PIO_UART_DEFAULT_BAUDRATE,
//         .rx_pin = 28,
//         .tx_pin = 26,
//         .id = 5,
//     },
//     .en_pin = 27,
// };

struct hw_uart *active_hw_uarts[COUNT_HW_UARTS + 1] = {NULL};
struct pio_uart *active_pio_uarts[COUNT_PIO_UARTS + 1] = {NULL};
