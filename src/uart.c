#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <FreeRTOS.h>
#include <stream_buffer.h>
#include <task.h>

#include "uart.h"

#include "debug.h"
#include "time.h"

// FIXME: Convert all Panic to some kind fo log in flash, so the watchdog could recovery the application, but the log will be stored.

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

// PIO Programs
#include "uart_tx.pio.h"
#include "uart_rx.pio.h"

//
// Prototypes
static void isr_hw_uart(void);

static void isr_pio_uart_tx_done(void);
static void isr_pio_uart_tx_fifo(void);

static void isr_pio_uart_rx(void);

static void hw_uart_fill_tx_fifo(struct hw_uart *uart);
static void hw_uart_fill_tx_fifo_from_isr(struct hw_uart *uart);
static void hw_uart_tx_fifo_irq_enabled(struct hw_uart *uart, bool enabled);

static void pio_uart_fill_tx_fifo(struct pio_uart *uart);
static void pio_uart_fill_tx_fifo_from_isr(struct pio_uart *uart);
static void pio_uart_tx_fifo_irq_enabled(struct pio_uart *uart, bool enabled);

//
// Hardware UARTs

struct hw_uart hw_uart1 = {
    .super = {
        .baudrate = HW_UART_DEFAULT_BAUDRATE,
        .rx_pin = 5,
        .tx_pin = 4,
        .id = 1,
    },
    .native_uart = uart1,
};

//
// RS485 UARTs

struct pio_uart pio_uart_1 = {
    .super = {
        .baudrate = PIO_UART_DEFAULT_BAUDRATE,
        .rx_pin = 7,
        .tx_pin = 8,
        .id = UART_PIO_ID(1),
    },
    .en_pin = 9,
};

struct pio_uart pio_uart_2 = {
    .super = {
        .baudrate = PIO_UART_DEFAULT_BAUDRATE,
        .rx_pin = 10,
        .tx_pin = 11,
        .id = UART_PIO_ID(2),
    },
    .en_pin = 12,
};

struct pio_uart pio_uart_3 = {
    .super = {
        .baudrate = PIO_UART_DEFAULT_BAUDRATE,
        .rx_pin = 13,
        .tx_pin = 14,
        .id = UART_PIO_ID(3),
    },
    .en_pin = 15,
};

struct pio_uart pio_uart_4 = {
    .super = {
        .baudrate = PIO_UART_DEFAULT_BAUDRATE,
        .rx_pin = 18,
        .tx_pin = 16,
        .id = UART_PIO_ID(4),
    },
    .en_pin = 17,
};

struct pio_uart pio_uart_5 = {
    .super = {
        .baudrate = PIO_UART_DEFAULT_BAUDRATE,
        .rx_pin = 19,
        .tx_pin = 20,
        .id = UART_PIO_ID(5),
    },
    .en_pin = 21,
};

struct pio_uart pio_uart_6 = {
    .super = {
        .baudrate = PIO_UART_DEFAULT_BAUDRATE,
        .rx_pin = 28,
        .tx_pin = 26,
        .id = UART_PIO_ID(6),
    },
    .en_pin = 27,
};

struct hw_uart *active_hw_uarts[NUM_HW_UARTS] = {NULL};

struct pio_uart *active_pio_uarts[NUM_PIO_UARTS] = {NULL};

volatile bool uart_activity;

//
// UARTs Initialization
//

void init_hw_uart(struct hw_uart *const hw_uart)
{
    // Add this UART to the active uart array
    active_hw_uarts[hw_uart->super.id] = hw_uart;

    // Initialize Hardware UART
    uart_init(hw_uart->native_uart, hw_uart->super.baudrate);
    gpio_set_function(hw_uart->super.tx_pin, GPIO_FUNC_UART);
    gpio_set_function(hw_uart->super.rx_pin, GPIO_FUNC_UART);
    gpio_set_pulls(hw_uart->super.tx_pin, true, false);
    gpio_set_pulls(hw_uart->super.rx_pin, true, false);
    uart_set_hw_flow(hw_uart->native_uart, false, false);
    uart_set_format(hw_uart->native_uart, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(hw_uart->native_uart, true);
    hw_uart->super.tx_sbuffer = xStreamBufferCreate(UARTS_BUFFER_SIZE, 1);
    hw_uart->super.rx_sbuffer = xStreamBufferCreate(UARTS_BUFFER_SIZE, 1);

    // Enable IRQ but keep it off until the Queue is filled
    irq_set_exclusive_handler(UART_IRQ_NUM(hw_uart->native_uart), isr_hw_uart);
    irq_set_enabled(UART_IRQ_NUM(hw_uart->native_uart), true);
    hw_uart_tx_fifo_irq_enabled(hw_uart, false);
}

void init_pio_uart(struct pio_uart *const pio_uart)
{
    static int rx_program_offset = PICO_ERROR_GENERIC;
    static int tx_program_offset = PICO_ERROR_GENERIC;

    static const enum irq_num_rp2040 rx_fifo_irq = PIO_IRQ_NUM(PIO_UART_RX_PIO, 0); // All RX use the same IRQ
    static const enum irq_num_rp2040 tx_done_irq = PIO_IRQ_NUM(PIO_UART_TX_PIO, 0); // All TX use the same IRQ
    static const enum irq_num_rp2040 tx_fifo_irq = PIO_IRQ_NUM(PIO_UART_TX_PIO, 1); // All TX use the same IRQ

    // Add this UART to the active uart array
    active_pio_uarts[pio_uart->super.id] = pio_uart;

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
    pio_uart->super.tx_sbuffer = xStreamBufferCreate(UARTS_BUFFER_SIZE, 1);
    pio_uart->tx_done = true;
    pio_uart->super.rx_sbuffer = xStreamBufferCreate(UARTS_BUFFER_SIZE, 1);
    pio_uart->super.rx_sbuffer_overrun = false;

    // Initialize RX FIFO not empty IRQ
    if (!irq_get_exclusive_handler(rx_fifo_irq))
    {
        irq_set_exclusive_handler(rx_fifo_irq, isr_pio_uart_rx);
        irq_set_enabled(rx_fifo_irq, true);
    }
    pio_set_irqn_source_enabled(pio_uart->rx_pio, PIO_UART_RX_FIFO_IRQ_INDEX, pis_sm0_rx_fifo_not_empty + pio_uart->rx_sm, true);

    // Initialize TX Done IRQ
    if (!irq_get_exclusive_handler(tx_done_irq))
    {
        irq_set_exclusive_handler(tx_done_irq, isr_pio_uart_tx_done);
        irq_set_enabled(tx_done_irq, true);
    }
    pio_set_irqn_source_enabled(pio_uart->tx_pio, PIO_UART_TX_DONE_IRQ_INDEX, pis_interrupt0 + pio_uart->tx_sm, true);

    // Initialize TX FIFO not full IRQ
    if (!irq_get_exclusive_handler(tx_fifo_irq))
    {
        irq_set_exclusive_handler(tx_fifo_irq, isr_pio_uart_tx_fifo);
        irq_set_enabled(tx_fifo_irq, true);
    }
    pio_uart_tx_fifo_irq_enabled(pio_uart, false);
}

//
// UARTs ISR
//

static inline bool hw_uart_is_tx_irq(struct hw_uart *uart)
{
    uint32_t irqFlags = uart_get_hw(uart->native_uart)->mis;

    return irqFlags & (UART_UARTMIS_TXMIS_BITS);
}

static inline bool hw_uart_is_rx_irq(struct hw_uart *uart)
{
    uint32_t irqFlags = uart_get_hw(uart->native_uart)->mis;

    return irqFlags & (UART_UARTMIS_RXMIS_BITS | UART_UARTMIS_RTMIS_BITS);
}

static inline void hw_uart_tx_fifo_irq_enabled(struct hw_uart *uart, bool tx_enabled)
{
    uart_set_irqs_enabled(uart->native_uart, true, tx_enabled);
}

// Same for TX and RX
static void isr_hw_uart(void)
{
    size_t size = 0;
    uint8_t data[UARTS_BUFFER_SIZE];

    for (size_t i = 0; i < NUM_HW_UARTS; i++)
    {
        if (active_hw_uarts[i] != NULL)
        {
            struct hw_uart *uart = active_hw_uarts[i];

            //
            // RX

            if (hw_uart_is_rx_irq(uart))
            {
                size = 0;
                while (uart_is_readable(uart->native_uart))
                {
                    // Read from the register, avoid double 'uart_is_readable' in 'uart_getc'
                    data[size++] = (uint8_t)uart_get_hw(uart->native_uart)->dr;
                }
                if (size)
                {
                    size_t written = xStreamBufferSendFromISR(uart->super.rx_sbuffer, data, size, NULL);
                    uart->super.rx_sbuffer_overrun |= written != size; // Check if we overrun the buffer

                    uart->super.activity = true;
                }
            }

            //
            // TX

            if (hw_uart_is_tx_irq(uart))
            {
                hw_uart_fill_tx_fifo_from_isr(uart);

                // if the StreamBuffer is empty, disable IRQ, no more data to send
                if (xStreamBufferIsEmpty(uart->super.tx_sbuffer))
                {
                    hw_uart_tx_fifo_irq_enabled(uart, false);
                }
            }
        }
    }
}

static inline void pio_uart_tx_fifo_irq_enabled(struct pio_uart *uart, bool enabled)
{
    pio_set_irqn_source_enabled(uart->tx_pio, PIO_UART_TX_FIFO_IRQ_INDEX, pis_sm0_tx_fifo_not_full + uart->tx_sm, enabled);
}

static void isr_pio_uart_tx_done(void)
{
    for (size_t i = 0; i < NUM_PIO_UARTS; i++)
    {
        if (active_pio_uarts[i] != NULL)
        {
            struct pio_uart *pio_uart = active_pio_uarts[i];

            if (pio_interrupt_get(pio_uart->tx_pio, pio_uart->tx_sm))
            {
                pio_uart->tx_done = true;
                pio_interrupt_clear(pio_uart->tx_pio, pio_uart->tx_sm);
            }
        }
    }
}

static void isr_pio_uart_tx_fifo(void)
{
    for (size_t i = 0; i < NUM_PIO_UARTS; i++)
    {
        if (active_pio_uarts[i] != NULL)
        {
            struct pio_uart *uart = active_pio_uarts[i];

            if (pio_uart_is_writable(uart))
            {
                pio_uart_fill_tx_fifo_from_isr(uart);

                if (xStreamBufferIsEmpty(uart->super.tx_sbuffer))
                {
                    pio_uart_tx_fifo_irq_enabled(uart, false);
                }
            }
        }
    }
}

static void isr_pio_uart_rx(void)
{
    size_t size = 0;
    uint8_t src[UARTS_BUFFER_SIZE];
    for (size_t i = 0; i < NUM_PIO_UARTS; i++)
    {
        if (active_pio_uarts[i] != NULL)
        {
            struct pio_uart *uart = active_pio_uarts[i];
            size = 0;
            while (!pio_rx_empty(uart))
            {
                src[size++] = pio_rx_getc(uart);
            }
            if (size > 0)
            {
                // Check if we overrun the buffer
                size_t written = xStreamBufferSendFromISR(uart->super.rx_sbuffer, src, size, NULL);
                uart->super.rx_sbuffer_overrun |= written != size;

                uart->super.activity = true;
            }
        }
    }
}

//
// UARTs Manipulation
//

static inline void hw_uart_fill_tx_fifo(struct hw_uart *uart)
{
    while (uart_is_writable(uart->native_uart) && !xStreamBufferIsEmpty(uart->super.tx_sbuffer))
    {
        uint8_t data;
        if (xStreamBufferReceive(uart->super.tx_sbuffer, &data, 1, 0))
        {
            uart_putc_raw(uart->native_uart, data);
        }
    }
}

static inline void hw_uart_fill_tx_fifo_from_isr(struct hw_uart *uart)
{
    while (uart_is_writable(uart->native_uart) && !xStreamBufferIsEmpty(uart->super.tx_sbuffer))
    {
        uint8_t data;
        if (xStreamBufferReceiveFromISR(uart->super.tx_sbuffer, &data, 1, NULL))
        {
            uart_putc_raw(uart->native_uart, data);
        }
    }
}

static inline void pio_uart_fill_tx_fifo(struct pio_uart *uart)
{
    while (pio_uart_is_writable(uart) && !xStreamBufferIsEmpty(uart->super.tx_sbuffer))
    {
        uint8_t data;
        if (xStreamBufferReceive(uart->super.tx_sbuffer, &data, 1, 0))
        {
            pio_uart_putc(uart, data);
        }
    }
}

static inline void pio_uart_fill_tx_fifo_from_isr(struct pio_uart *uart)
{
    while (pio_uart_is_writable(uart) && !xStreamBufferIsEmpty(uart->super.tx_sbuffer))
    {
        uint8_t data;
        if (xStreamBufferReceiveFromISR(uart->super.tx_sbuffer, &data, 1, NULL))
        {
            pio_uart_putc(uart, data);
        }
    }
}

inline size_t hw_uart_write_bytes(struct hw_uart *const uart, const void *src, size_t size)
{
    if (size)
    {
        hw_uart_fill_tx_fifo(uart);
        hw_uart_tx_fifo_irq_enabled(uart, true);

        size_t written = xStreamBufferSend(uart->super.tx_sbuffer, src, size, 0);

        uart->super.tx_sbuffer_overrun |= (written != size);
        uart->super.activity = true;

        return written;
    }
    return 0;
}

inline size_t pio_uart_write_bytes(struct pio_uart *const uart, const void *src, size_t size)
{
    if (size)
    {
        pio_uart_fill_tx_fifo(uart);
        pio_uart_tx_fifo_irq_enabled(uart, true);

        size_t written = xStreamBufferSend(uart->super.tx_sbuffer, src, size, 0);

        uart->super.tx_sbuffer_overrun |= (written != size);
        uart->tx_done = false;
        uart->super.activity = true;

        return written;
    }
    return 0;
}

inline size_t hw_uart_read_bytes(struct hw_uart *const uart, void *dst, uint8_t size)
{
    return xStreamBufferReceive(uart->super.rx_sbuffer, dst, size, 0);
}

inline size_t pio_uart_read_bytes(struct pio_uart *const uart, void *dst, uint8_t size)
{
    return xStreamBufferReceive(uart->super.rx_sbuffer, dst, size, 0);
}

//
// Maintenance Task

static inline void check_overrrun(struct uart *uart)
{
    if (uart->rx_sbuffer_overrun)
    {
        uart->rx_sbuffer_overrun = false;

        printf("[WARN] %s UART %lu RX Overrun.\n", UART_TYPE(uart->id), UART_ID(uart->id));
    }
    if (uart->tx_sbuffer_overrun)
    {
        uart->tx_sbuffer_overrun = false;
        printf("[WARN] %s UART %lu TX Overrun.\n", UART_TYPE(uart->id), UART_ID(uart->id));
    }
}

static void task_uart_maintenance(void *arg)
{
    (void)arg;

    while (true)
    {
        //
        // Hardware UARTs
        for (size_t i = 0; i < NUM_HW_UARTS; i++)
        {
            if (active_hw_uarts[i] != NULL)
            {
                check_overrrun(&active_hw_uarts[i]->super);

                uart_activity |= active_hw_uarts[i]->super.activity;
                active_hw_uarts[i]->super.activity = false;
            }
        }

        //
        // PIO UARTs

        for (size_t i = 0; i < NUM_PIO_UARTS; i++)
        {
            if (active_pio_uarts[i] != NULL)
            {
                check_overrrun(&active_pio_uarts[i]->super);

                uart_activity |= active_pio_uarts[i]->super.activity;
                active_pio_uarts[i]->super.activity = false;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(25));
    }
}

void init_uart_maintenance(void)
{
    xTaskCreate(task_uart_maintenance, "UART Maintenance", configMINIMAL_STACK_SIZE, NULL, tskLOW_PRIORITY, NULL);
}
