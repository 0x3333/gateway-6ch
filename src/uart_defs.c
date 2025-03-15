#include "uart.h"
#include "defs.h"
//
// Hardware UARTs

struct hw_uart hw_uart1 = {
    .super = {
        .type = "HW",
        .baudrate = HW_UART_DEFAULT_BAUDRATE,
        .rx_pin = HW_UART_1_RX_PIN,
        .tx_pin = HW_UART_1_TX_PIN,
        .id = HW_UART_1_ID,
    },
    .native_uart = uart1,
};

//
// RS485 UARTs

struct pio_uart pio_uart_0 = {
    .super = {
        .type = "PIO",
        .baudrate = PIO_UART_DEFAULT_BAUDRATE,
        .rx_pin = BUS_1_RX_PIN,
        .tx_pin = BUS_1_TX_PIN,
        .id = BUS_1_ID,
    },
    .en_pin = BUS_1_EN_PIN,
};

struct pio_uart pio_uart_1 = {
    .super = {
        .type = "PIO",
        .baudrate = PIO_UART_DEFAULT_BAUDRATE,
        .rx_pin = BUS_2_RX_PIN,
        .tx_pin = BUS_2_TX_PIN,
        .id = BUS_2_ID,
    },
    .en_pin = BUS_2_EN_PIN,
};

struct pio_uart pio_uart_2 = {
    .super = {
        .type = "PIO",
        .baudrate = PIO_UART_DEFAULT_BAUDRATE,
        .rx_pin = BUS_3_RX_PIN,
        .tx_pin = BUS_3_TX_PIN,
        .id = BUS_3_ID,
    },
    .en_pin = BUS_3_EN_PIN,
};

struct pio_uart pio_uart_3 = {
    .super = {
        .type = "PIO",
        .baudrate = PIO_UART_DEFAULT_BAUDRATE,
        .rx_pin = BUS_4_RX_PIN,
        .tx_pin = BUS_4_TX_PIN,
        .id = BUS_4_ID,
    },
    .en_pin = BUS_4_EN_PIN,
};

struct pio_uart pio_uart_4 = {
    .super = {
        .type = "PIO",
        .baudrate = PIO_UART_DEFAULT_BAUDRATE,
        .rx_pin = BUS_5_RX_PIN,
        .tx_pin = BUS_5_TX_PIN,
        .id = BUS_5_ID,
    },
    .en_pin = BUS_5_EN_PIN,
};

//
// Note: Bus 6 is now associated with DMX
//
// struct pio_uart pio_uart_5 = {
//     .super = {
//         .type = "PIO",
//         .baudrate = PIO_UART_DEFAULT_BAUDRATE,
//         .rx_pin = BUS_6_RX_PIN,
//         .tx_pin = BUS_6_TX_PIN,
//         .id = BUS_6_ID,
//     },
//     .en_pin = BUS_6_EN_PIN,
// };

struct hw_uart *active_hw_uarts[COUNT_HW_UARTS + 1] = {NULL};
struct pio_uart *active_pio_uarts[COUNT_PIO_UARTS + 1] = {NULL};
