#include <stdio.h>

#include <FreeRTOS.h>
#include <task.h>

#include "uart.h"

#include "debug.h"
#include "time.h"

// PIO Programs
#include "uart_tx.pio.h"
#include "uart_rx.pio.h"

// FIXME: Convert all errors/issues to some kind of log in flash, so the watchdog could recovery the application, but the log will be stored.

//
// Prototypes
static void hw_uart_isr(void);

static void pio_uart_tx_done_isr(void);
static void pio_uart_tx_fifo_isr(void);

static void pio_uart_rx_isr(void);

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

struct pio_uart pio_uart_5 = {
    .super = {
        .type = "PIO",
        .baudrate = PIO_UART_DEFAULT_BAUDRATE,
        .rx_pin = 28,
        .tx_pin = 26,
        .id = 5,
    },
    .en_pin = 27,
};

struct hw_uart *active_hw_uarts[COUNT_HW_UARTS] = {NULL};

struct pio_uart *active_pio_uarts[COUNT_PIO_UARTS] = {NULL};

volatile bool uart_activity;

//
// UARTs Initialization
//

void hw_uart_init(struct hw_uart *const hw_uart)
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

    // Initialize Ring Buffers
    size_t buf_size = UARTS_BUFFER_SIZE * sizeof(uint8_t);
    uint8_t *txbuf = pvPortMalloc(buf_size);
    uint8_t *rxbuf = pvPortMalloc(buf_size);
    ring_buffer_init(&hw_uart->super.tx_rbuffer, txbuf, buf_size);
    ring_buffer_init(&hw_uart->super.rx_rbuffer, rxbuf, buf_size);

    // Enable IRQ but keep it off until the Queue is filled
    irq_set_exclusive_handler(UART_IRQ_NUM(hw_uart->native_uart), hw_uart_isr);
    irq_set_enabled(UART_IRQ_NUM(hw_uart->native_uart), true);
    hw_uart_tx_fifo_irq_enabled(hw_uart, false);
}

void pio_uart_init(struct pio_uart *const pio_uart)
{
    static int rx_program_offset = PICO_ERROR_GENERIC;
    static int tx_program_offset = PICO_ERROR_GENERIC;

    static const enum irq_num_rp2040 rx_fifo_irq = PIO_IRQ_NUM(PIO_UART_RX_PIO, 0); // All RX use the same FIFO IRQ
    static const enum irq_num_rp2040 tx_done_irq = PIO_IRQ_NUM(PIO_UART_TX_PIO, 0); // All TX use the same DONE IRQ
    static const enum irq_num_rp2040 tx_fifo_irq = PIO_IRQ_NUM(PIO_UART_TX_PIO, 1); // All TX use the same FIFO IRQ

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

    // Initialize Ring Buffers
    size_t buf_size = UARTS_BUFFER_SIZE * sizeof(uint8_t);
    uint8_t *txbuf = pvPortMalloc(buf_size);
    uint8_t *rxbuf = pvPortMalloc(buf_size);
    ring_buffer_init(&pio_uart->super.tx_rbuffer, txbuf, buf_size);
    ring_buffer_init(&pio_uart->super.rx_rbuffer, rxbuf, buf_size);

    pio_uart->tx_done = true;
    pio_uart->super.tx_rbuffer_overrun = false;
    pio_uart->super.rx_rbuffer_overrun = false;

    // Initialize RX FIFO not empty IRQ
    if (!irq_get_exclusive_handler(rx_fifo_irq))
    {
        irq_set_exclusive_handler(rx_fifo_irq, pio_uart_rx_isr);
        irq_set_enabled(rx_fifo_irq, true);
    }
    pio_set_irqn_source_enabled(pio_uart->rx_pio, PIO_UART_RX_FIFO_IRQ_INDEX, pis_sm0_rx_fifo_not_empty + pio_uart->rx_sm, true);

    // Initialize TX Done IRQ
    if (!irq_get_exclusive_handler(tx_done_irq))
    {
        irq_set_exclusive_handler(tx_done_irq, pio_uart_tx_done_isr);
        irq_set_enabled(tx_done_irq, true);
    }
    pio_set_irqn_source_enabled(pio_uart->tx_pio, PIO_UART_TX_DONE_IRQ_INDEX, pis_interrupt0 + pio_uart->tx_sm, true);

    // Initialize TX FIFO not full IRQ
    if (!irq_get_exclusive_handler(tx_fifo_irq))
    {
        irq_set_exclusive_handler(tx_fifo_irq, pio_uart_tx_fifo_isr);
        irq_set_enabled(tx_fifo_irq, true);
    }
    pio_uart_tx_fifo_irq_enabled(pio_uart, false);
}

// This is *stupid*, we should create an array with the pio_uart_X variables.
struct pio_uart *get_pio_uart_by_index(uint8_t index)
{
    switch (index)
    {
    case 0:
        return &pio_uart_0;
    case 1:
        return &pio_uart_1;
    case 2:
        return &pio_uart_2;
    case 3:
        return &pio_uart_3;
    case 4:
        return &pio_uart_4;
    case 5:
        return &pio_uart_5;
    default:
        return NULL;
    }
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
static void hw_uart_isr(void)
{
    for (size_t i = 0; i < COUNT_HW_UARTS; i++)
    {
        if (active_hw_uarts[i] != NULL)
        {
            struct hw_uart *uart = active_hw_uarts[i];

            //
            // RX

            if (hw_uart_is_rx_irq(uart))
            {
                while (uart_is_readable(uart->native_uart))
                {
                    // Read from the register, avoid double 'uart_is_readable' in 'uart_getc'
                    // TODO: Check if uart_is_readable is really a double call
                    uint8_t data = (uint8_t)uart_get_hw(uart->native_uart)->dr;
                    bool ret = ring_buffer_put(&uart->super.rx_rbuffer, data);
                    uart->super.rx_rbuffer_overrun |= !ret; // Check if we overrun the buffer
                    uart->super.activity = true;
                }
            }

            //
            // TX

            if (hw_uart_is_tx_irq(uart))
            {
                hw_uart_fill_tx_fifo_from_isr(uart);

                // if the Ring Buffer is empty, disable IRQ, no more data to send
                if (ring_buffer_is_empty(&uart->super.tx_rbuffer))
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

static void pio_uart_tx_done_isr(void)
{
    for (size_t i = 0; i < COUNT_PIO_UARTS; i++)
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

static void pio_uart_tx_fifo_isr(void)
{
    for (size_t i = 0; i < COUNT_PIO_UARTS; i++)
    {
        if (active_pio_uarts[i] != NULL)
        {
            struct pio_uart *uart = active_pio_uarts[i];

            if (pio_uart_is_writable(uart))
            {
                pio_uart_fill_tx_fifo_from_isr(uart);

                if (ring_buffer_is_empty(&uart->super.tx_rbuffer))
                {
                    pio_uart_tx_fifo_irq_enabled(uart, false);
                }
            }
        }
    }
}

static void pio_uart_rx_isr(void)
{
    for (size_t i = 0; i < COUNT_PIO_UARTS; i++)
    {
        struct pio_uart *uart = active_pio_uarts[i];
        if (uart != NULL &&
            pio_interrupt_get(uart->rx_pio, uart->rx_sm))
        {
            while (!pio_rx_empty(uart))
            {
                uint8_t data = pio_rx_getc(uart);
                bool ret = ring_buffer_put(&uart->super.rx_rbuffer, data);
                uart->super.rx_rbuffer_overrun |= !ret;
                uart->super.activity = true;
            }
            pio_interrupt_clear(uart->rx_pio, uart->rx_sm);
        }
    }
}

//
// UARTs Manipulation
//

static inline void hw_uart_fill_tx_fifo(struct hw_uart *uart)
{
    while (uart_is_writable(uart->native_uart) && !ring_buffer_is_empty(&uart->super.tx_rbuffer))
    {
        uint8_t data;
        if (ring_buffer_get(&uart->super.tx_rbuffer, &data))
        {
            uart_putc_raw(uart->native_uart, data);
        }
    }
}

static inline void hw_uart_fill_tx_fifo_from_isr(struct hw_uart *uart)
{
    while (uart_is_writable(uart->native_uart) && !ring_buffer_is_empty(&uart->super.tx_rbuffer))
    {
        uint8_t data;
        if (ring_buffer_get(&uart->super.tx_rbuffer, &data))
        {
            uart_putc_raw(uart->native_uart, data);
        }
    }
}

// TODO: Verificar a questão do acesso dentro e fora de ISR

static inline void pio_uart_fill_tx_fifo(struct pio_uart *uart)
{
    uint8_t data;
    while (pio_uart_is_writable(uart) && !ring_buffer_is_empty(&uart->super.tx_rbuffer))
    {
        if (ring_buffer_get(&uart->super.tx_rbuffer, &data))
        {
            pio_uart_putc(uart, data);
        }
    }
}

static inline void pio_uart_fill_tx_fifo_from_isr(struct pio_uart *uart)
{
    uint8_t data;
    while (pio_uart_is_writable(uart) && !ring_buffer_is_empty(&uart->super.tx_rbuffer))
    {
        if (ring_buffer_get(&uart->super.tx_rbuffer, &data))
        {
            pio_uart_putc(uart, data);
        }
    }
}

// Write

inline size_t hw_uart_write_bytes(struct hw_uart *const uart, const void *src, size_t size)
{
    size_t written;
    for (written = 0; written < size; written++)
    {
        bool ret = ring_buffer_put(&uart->super.tx_rbuffer, *((uint8_t *)src + written));
        uart->super.tx_rbuffer_overrun |= !ret;
    }
    if (written)
    {
        hw_uart_fill_tx_fifo(uart);
        hw_uart_tx_fifo_irq_enabled(uart, true);

        uart->super.activity = true;
    }

    return written;
}

inline size_t pio_uart_write_bytes(struct pio_uart *const uart, const void *src, size_t size)
{
    size_t written;
    for (written = 0; written < size; written++)
    {
        bool ret = ring_buffer_put(&uart->super.tx_rbuffer, *((uint8_t *)src + written));
        uart->super.tx_rbuffer_overrun |= !ret;
    }
    if (written)
    {
        pio_uart_fill_tx_fifo(uart);
        pio_uart_tx_fifo_irq_enabled(uart, true);

        uart->super.activity = true;
    }
    return written;
}

// Read

// TODO: Criar uma versão de gets and puts no ring buffer.

inline size_t hw_uart_read_bytes(struct hw_uart *const uart, void *dst, uint8_t size)
{
    size_t read;
    for (read = 0; read < size; read++)
    {
        if (!ring_buffer_get(&uart->super.rx_rbuffer, (uint8_t *)dst + read))
        {
            break;
        }
    }
    return read;
}

inline size_t pio_uart_read_bytes(struct pio_uart *const uart, void *dst, uint8_t size)
{
    size_t read;
    for (read = 0; read < size; read++)
    {
        if (!ring_buffer_get(&uart->super.rx_rbuffer, (uint8_t *)dst + read))
        {
            break;
        }
    }
    return read;
}

// Flush

inline void hw_uart_flush_rx(struct hw_uart *const uart)
{
    while (uart_is_readable(uart->native_uart))
    {
        // Read from the register, avoid double 'uart_is_readable' in 'uart_getc'
        // TODO: Check if uart_is_readable is really a double call
        volatile uint8_t dummy = (uint8_t)uart_get_hw(uart->native_uart)->dr;
        (void)dummy;
    }
    ring_buffer_clear(&uart->super.rx_rbuffer);
}

inline void pio_uart_flush_rx(struct pio_uart *const uart)
{
    while (!pio_rx_empty(uart))
    {
        pio_rx_getc(uart);
    }
    ring_buffer_clear(&uart->super.rx_rbuffer);
}

//
// Maintenance Task

static inline void check_overrrun(struct uart *uart)
{
    if (uart->rx_rbuffer_overrun)
    {
        uart->rx_rbuffer_overrun = false;

        printf("[WARN] %s UART %lu RX Overrun.\n", uart->type, uart->id);
    }
    if (uart->tx_rbuffer_overrun)
    {
        uart->tx_rbuffer_overrun = false;
        printf("[WARN] %s UART %lu TX Overrun.\n", uart->type, uart->id);
    }
}

static void task_uart_maintenance(void *arg)
{
    (void)arg;

    while (true)
    {
        //
        // Hardware UARTs
        for (size_t i = 0; i < COUNT_HW_UARTS; i++)
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

        for (size_t i = 0; i < COUNT_PIO_UARTS; i++)
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

void uart_maintenance_init(void)
{
    xTaskCreate(task_uart_maintenance, "UART Maintenance", configMINIMAL_STACK_SIZE, NULL, tskLOW_PRIORITY, NULL);
}
