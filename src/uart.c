#include <stdio.h>

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include "macrologger.h"

#include "uart.h"
#include "utils.h"

// PIO Programs
#include "uart_tx.pio.h"
#include "uart_rx.pio.h"

//
// Prototypes
static void hw_uart_rx_isr(void);

static void pio_uart_tx_done_isr(void);
static void pio_uart_tx_fifo_isr(void);
static void pio_uart_tx_fifo_irq_enabled(struct pio_uart *uart, bool enabled);
static void pio_uart_rx_isr(void);

volatile bool uart_activity;

//
// UARTs Initialization
//

static ssize_t add_active_hw_uart(struct hw_uart *uart)
{
    size_t i;
    for (i = 0; i < COUNT_HW_UARTS; i++)
    {
        if (active_hw_uarts[i] == NULL)
        {
            active_hw_uarts[i] = uart;
            return i;
        }
    }
    return -1;
}

static ssize_t add_active_pio_uart(struct pio_uart *uart)
{
    size_t i;
    for (i = 0; i < COUNT_PIO_UARTS; i++)
    {
        if (active_pio_uarts[i] == NULL)
        {
            active_pio_uarts[i] = uart;
            return i;
        }
    }
    return -1;
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
    default:
        return NULL;
    }
}

void hw_uart_init(struct hw_uart *const hw_uart)
{
    // Add this UART to the active uart array
    add_active_hw_uart(hw_uart);

    // Initialize Hardware UART
    uart_init(hw_uart->native_uart, hw_uart->super.baudrate);
    gpio_set_function(hw_uart->super.tx_pin, GPIO_FUNC_UART);
    gpio_set_function(hw_uart->super.rx_pin, GPIO_FUNC_UART);
    gpio_set_pulls(hw_uart->super.tx_pin, true, false);
    gpio_set_pulls(hw_uart->super.rx_pin, true, false);
    uart_set_hw_flow(hw_uart->native_uart, false, false);
    uart_set_format(hw_uart->native_uart, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(hw_uart->native_uart, true);

    // Initialize Buffers and Mutex
    hw_uart->super.tx_buffer = xStreamBufferCreate(UART_BUFFER_SIZE, sizeof(uint8_t));
    hw_uart->super.tx_buffer_mutex = xSemaphoreCreateMutex();
    hw_uart->super.tx_buffer_overrun = false;
    hw_uart->super.tx_done = true;
    hw_uart->super.rx_buffer = xStreamBufferCreate(UART_BUFFER_SIZE, sizeof(uint8_t));
    hw_uart->super.rx_buffer_mutex = xSemaphoreCreateMutex();
    hw_uart->super.rx_buffer_overrun = false;

    // Enable IRQ but keep it off until the Queue is filled
    irq_set_exclusive_handler(UART_IRQ_NUM(hw_uart->native_uart), hw_uart_rx_isr);
    irq_set_enabled(UART_IRQ_NUM(hw_uart->native_uart), true); // NVIC
    uart_set_irqs_enabled(hw_uart->native_uart, true, false);  // RX Always enabled, TX Disabled
}

void pio_uart_init(struct pio_uart *const pio_uart)
{
    static int rx_program_offset = PICO_ERROR_GENERIC;
    static int tx_program_offset = PICO_ERROR_GENERIC;

    static const enum irq_num_rp2040 rx_fifo_irq = PIO_IRQ_NUM(PIO_UART_RX_PIO, PIO_UART_RX_FIFO_IRQ_INDEX); // All RX use the same FIFO IRQ
    static const enum irq_num_rp2040 tx_fifo_irq = PIO_IRQ_NUM(PIO_UART_TX_PIO, PIO_UART_TX_FIFO_IRQ_INDEX); // All TX use the same FIFO IRQ
    static const enum irq_num_rp2040 tx_done_irq = PIO_IRQ_NUM(PIO_UART_TX_PIO, PIO_UART_TX_DONE_IRQ_INDEX); // All TX use the same DONE IRQ

    // Add this UART to the active uart array
    add_active_pio_uart(pio_uart);

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

    // Initialize Buffers and Mutex
    pio_uart->super.tx_buffer = xStreamBufferCreate(UART_BUFFER_SIZE, sizeof(uint8_t));
    pio_uart->super.tx_buffer_mutex = xSemaphoreCreateMutex();
    pio_uart->super.tx_buffer_overrun = false;
    pio_uart->super.tx_done = true;

    pio_uart->super.rx_buffer = xStreamBufferCreate(UART_BUFFER_SIZE, sizeof(uint8_t));
    pio_uart->super.rx_buffer_mutex = xSemaphoreCreateMutex();
    pio_uart->super.rx_buffer_overrun = false;

    // Initialize RX FIFO not empty IRQ
    if (!irq_get_exclusive_handler(rx_fifo_irq))
    {
        irq_set_exclusive_handler(rx_fifo_irq, pio_uart_rx_isr);
        irq_set_enabled(rx_fifo_irq, true);
    }
    pio_set_irqn_source_enabled(pio_uart->rx_pio,
                                PIO_UART_RX_FIFO_IRQ_INDEX,
                                pis_sm0_rx_fifo_not_empty + pio_uart->rx_sm,
                                true);

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
        irq_set_enabled(tx_fifo_irq, true); // ... then, enabled in the NVIC
    }
    pio_uart_tx_fifo_irq_enabled(pio_uart, false); // Needs to be disabled...
}

//
// UARTs ISR
//

// Same for TX and RX
static void hw_uart_rx_isr(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint8_t data;
    for (size_t i = 0; active_hw_uarts[i] != NULL; i++)
    {
        struct hw_uart *uart = active_hw_uarts[i];
        BaseType_t taskWoken = pdFALSE;

        //
        // RX
        while (uart_is_readable(uart->native_uart))
        {
            // Read from the register, avoid double 'uart_is_readable' in 'uart_getc'
            data = (uint8_t)uart_get_hw(uart->native_uart)->dr;
            bool ret = xStreamBufferSendFromISR(uart->super.rx_buffer, &data, 1, &taskWoken);
            uart->super.rx_buffer_overrun |= !ret; // Check if we overrun the buffer
        }
        uart->super.activity = true;
        // Check for hardware overrun
        uart->super.rx_buffer_overrun |= uart_get_hw(uart->native_uart)->ris & UART_UARTRIS_OERIS_BITS;
        xHigherPriorityTaskWoken |= taskWoken;
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static inline void pio_uart_tx_fifo_irq_enabled(struct pio_uart *uart, bool enabled)
{
    pio_set_irqn_source_enabled(uart->tx_pio,
                                PIO_UART_TX_FIFO_IRQ_INDEX,
                                pis_sm0_tx_fifo_not_full + uart->tx_sm,
                                enabled);
}

static void pio_uart_tx_done_isr(void)
{
    for (size_t i = 0; active_pio_uarts[i] != NULL; i++)
    {
        struct pio_uart *uart = active_pio_uarts[i];
        if (pio_interrupt_get(uart->tx_pio, uart->tx_sm))
        {
            uart->super.tx_done = true;
            pio_interrupt_clear(uart->tx_pio, uart->tx_sm);
        }
    }
}

static void pio_uart_tx_fifo_isr(void)
{
    uint8_t data;
    for (size_t i = 0; active_pio_uarts[i] != NULL; i++)
    {
        struct pio_uart *uart = active_pio_uarts[i];
        if (pio_uart_is_writable(uart))
        {
            while (pio_uart_is_writable(uart) && !xStreamBufferIsEmpty(uart->super.tx_buffer))
            {
                if (xStreamBufferReceiveFromISR(uart->super.tx_buffer, &data, 1, NULL))
                {
                    pio_uart_putc(uart, data);
                }
            }

            if (xStreamBufferIsEmpty(uart->super.tx_buffer))
            {
                pio_uart_tx_fifo_irq_enabled(uart, false);
            }
        }
    }
}

static void pio_uart_rx_isr(void)
{
    for (size_t i = 0; active_pio_uarts[i] != NULL; i++)
    {
        struct pio_uart *uart = active_pio_uarts[i];
        while (!pio_rx_empty(uart))
        {
            uint8_t data = pio_rx_getc(uart);
            bool ret = xStreamBufferSendFromISR(uart->super.rx_buffer, &data, 1, NULL);
            uart->super.rx_buffer_overrun |= !ret;
            uart->super.activity = true;
        }
    }
}

//
// UARTs Manipulation
//

// Write
static inline size_t _uart_write_bytes(struct uart *const uart, const void *src, size_t size, bool blocking)
{
    size_t bytes_written = xStreamBufferSend(uart->tx_buffer, src, size, blocking ? portMAX_DELAY : FREERTOS_NO_WAIT);
    uart->tx_buffer_overrun |= bytes_written != size;
    uart->activity = true;
    uart->tx_done = false;
    return bytes_written;
}

inline size_t hw_uart_write_bytes_blocking(struct hw_uart *const uart, const void *src, size_t size)
{
    size_t bytes_written = _uart_write_bytes(&uart->super, src, size, true);
    return bytes_written;
}

inline size_t pio_uart_write_bytes_blocking(struct pio_uart *const uart, const void *src, size_t size)
{
    size_t bytes_written = _uart_write_bytes(&uart->super, src, size, true);
    pio_uart_tx_fifo_irq_enabled(uart, true);
    return bytes_written;
}

// Read

static inline size_t _uart_read_bytes(struct uart *const uart, void *dst, uint8_t size, bool blocking)
{
    return xStreamBufferReceive(uart->rx_buffer, (uint8_t *)dst, size, (blocking ? portMAX_DELAY : FREERTOS_NO_WAIT));
}

inline size_t hw_uart_read_bytes(struct hw_uart *const uart, void *dst, uint8_t size)
{
    return _uart_read_bytes(&uart->super, dst, size, false);
}

inline size_t hw_uart_read_bytes_blocking(struct hw_uart *const uart, void *dst, uint8_t size)
{
    return _uart_read_bytes(&uart->super, dst, size, true);
}

inline size_t pio_uart_read_bytes(struct pio_uart *const uart, void *dst, uint8_t size)
{
    return _uart_read_bytes(&uart->super, dst, size, false);
}

inline size_t pio_uart_read_bytes_blocking(struct pio_uart *const uart, void *dst, uint8_t size)
{
    return _uart_read_bytes(&uart->super, dst, size, true);
}

// Flush

inline void hw_uart_rx_flush(struct hw_uart *const uart)
{
    if (xSemaphoreTake(uart->super.tx_buffer_mutex, portMAX_DELAY))
    {
        while (uart_is_readable(uart->native_uart))
        {
            // Read from the register, avoid double 'uart_is_readable' in 'uart_getc'
            // TODO: Check if uart_is_readable is really a double call
            volatile uint8_t dummy = (uint8_t)uart_get_hw(uart->native_uart)->dr;
            (void)dummy;
        }
        xStreamBufferReset(uart->super.rx_buffer);
        xSemaphoreGive(uart->super.tx_buffer_mutex);
    }
}

inline void pio_uart_rx_flush(struct pio_uart *const uart)
{
    if (xSemaphoreTake(uart->super.tx_buffer_mutex, portMAX_DELAY))
    {
        while (!pio_rx_empty(uart))
        {
            pio_rx_getc(uart);
        }
        xStreamBufferReset(uart->super.rx_buffer);
        xSemaphoreGive(uart->super.tx_buffer_mutex);
    }
}

// Others

inline size_t hw_uart_tx_buffer_remaining(const struct hw_uart *uart)
{
    return xStreamBufferSpacesAvailable(uart->super.tx_buffer);
}

inline size_t pio_uart_tx_buffer_remaining(const struct pio_uart *uart)
{
    return xStreamBufferSpacesAvailable(uart->super.tx_buffer);
}

//
// Maintenance Task

static inline void check_overrun(struct uart *uart)
{
    if (uart->rx_buffer_overrun)
    {
        uart->rx_buffer_overrun = false;

        LOG_ERROR("[WARN] %s UART %lu RX Overrun.", uart->type, uart->id);
    }
    if (uart->tx_buffer_overrun)
    {
        uart->tx_buffer_overrun = false;
        LOG_ERROR("[WARN] %s UART %lu TX Overrun.", uart->type, uart->id);
    }
}

_Noreturn static void task_uart_maintenance(void *arg)
{
    (void)arg;

    for (;;) // Task infinite loop
    {
        //
        // Hardware UARTs
        for (size_t i = 0; active_hw_uarts[i] != NULL; i++)
        {
            check_overrun(&active_hw_uarts[i]->super);

            uart_activity |= active_hw_uarts[i]->super.activity;
            active_hw_uarts[i]->super.activity = false;
        }

        //
        // PIO UARTs

        for (size_t i = 0; active_pio_uarts[i] != NULL; i++)
        {
            check_overrun(&active_pio_uarts[i]->super);

            uart_activity |= active_pio_uarts[i]->super.activity;
            active_pio_uarts[i]->super.activity = false;
        }

        vTaskDelay(pdMS_TO_TICKS(25));
    }
}

_Noreturn static void task_hw_uart_tx(void *arg)
{
    (void)arg;
    uint8_t data;

    for (;;) // Task infinite loop
    {
        //
        // Hardware UARTs
        for (size_t i = 0; active_hw_uarts[i] != NULL; i++)
        {
            struct hw_uart *uart = active_hw_uarts[i];

            if (xSemaphoreTake(uart->super.tx_buffer_mutex, portMAX_DELAY))
            {
                size_t bytes_written = 0;
                while (uart_is_writable(uart->native_uart) && !xStreamBufferIsEmpty(uart->super.tx_buffer))
                {
                    if (xStreamBufferReceive(uart->super.tx_buffer, &data, 1, FREERTOS_NO_WAIT))
                    {
                        uart_putc_raw(uart->native_uart, data);
                        bytes_written++;
                    }
                }
                uart->super.activity |= bytes_written;
                uart->super.tx_done = xStreamBufferIsEmpty(uart->super.tx_buffer);
                xSemaphoreGive(uart->super.tx_buffer_mutex);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void uart_maintenance_init(void)
{
    LOG_DEBUG("Initializing UART Maintenance Task");

    xTaskCreateAffinitySet(task_hw_uart_tx,
                           "Hw UART TX",
                           configMINIMAL_STACK_SIZE,
                           NULL,
                           tskLOW_PRIORITY,
                           HOST_TASK_CORE_AFFINITY,
                           NULL);
    xTaskCreateAffinitySet(task_uart_maintenance,
                           "UART Maintenance",
                           configMINIMAL_STACK_SIZE,
                           NULL,
                           tskLOW_PRIORITY,
                           BUS_TASK_CORE_AFFINITY,
                           NULL);
}
