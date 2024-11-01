#include <stdio.h>
#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <task.h>
#include <target/min.h>

#include "debug.h"

#include "uart.h"
#include "modbus.h"
#include "protocol.h"
#include "esp.h"
#include "led.h"

static void task_flash_act_led(void *arg);
static void task_pio_uart_tx(void *arg);
static void task_pio_uart_rx(void *arg);
static void task_comm_esp(void *arg);

int main()
{
#ifdef DEBUG_BUILD
    // Allow core1 to launch in debugger.
    timer_hw->dbgpause = 0;
#endif

#if LIB_PICO_STDIO_UART
    // If using UART as default output, initialize it first
    init_hw_uart(&esp_uart);
#endif

    stdio_init_all();

#if LIB_PICO_STDIO_USB
    // Wait for 2 seconds USB CDC to connect, otherwise, continue
    uint_fast8_t wait = 0;
    while (!stdio_usb_connected())
    {
        wait++;
        if (wait >= 2000 / 8)
            break;
        sleep_ms(8);
    }
#endif

    // Header
    printf("\n");
    printf("   ___   ____ ____  ___   ____   _____       __                         \n");
    printf("  / _ \\ / __// / / ( _ ) / __/  / ___/___ _ / /_ ___  _    __ ___ _ __ __\n");
    printf(" / , _/_\\ \\ /_  _// _  |/__ \\  / (_ // _ `// __// -_)| |/|/ // _ `// // /\n");
    printf("/_/|_|/___/  /_/  \\___//____/  \\___/ \\_,_/ \\__/ \\__/ |__,__/ \\_,_/ \\_, / \n");
    printf("                                                                  /___/  \n");
    printf("By: Tercio Gaudencio Filho - %s %s\n\n", __DATE__, __TIME__);

    // Init Peripherals
    printf("Initializing Peripherals...\n");
#if !LIB_PICO_STDIO_UART
    init_hw_uart(&esp_uart);
#endif

    init_pio_uart(&pio_uart_bus_3);
    init_pio_uart(&pio_uart_bus_4);
    init_pio_uart(&pio_uart_bus_5);
    init_pio_uart(&pio_uart_bus_6);

    // Init Tasks
    printf("Creating tasks...\n");
    xTaskCreate(task_comm_esp, "Comm-ESP", 1024, NULL, tskDEFAULT_PRIORITY, NULL);
    xTaskCreate(task_pio_uart_tx, "UART-TX", 1024, NULL, tskDEFAULT_PRIORITY, NULL);
    xTaskCreate(task_pio_uart_rx, "UART-RX", 1024, NULL, tskDEFAULT_PRIORITY, NULL);
    xTaskCreate(task_flash_act_led, "ACT-LED", configMINIMAL_STACK_SIZE, NULL, tskLOW_PRIORITY, NULL);

    printf("Running\n\n");
    vTaskStartScheduler();

    while (true)
        tight_loop_contents();
}

struct min_context min_ctx;

static void task_flash_act_led(void *arg)
{
    (void)arg;

    gpio_init(ACT_LED_PIN);
    gpio_set_dir(ACT_LED_PIN, GPIO_OUT);

    while (true)
    {
        gpio_xor_mask(1u << ACT_LED_PIN);

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void task_comm_esp(void *arg)
{
    (void)arg;

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    struct p_config_mb_read payload = {
        .base = {
            .bus = 1,
            .mb_slave = 0x2,
            .mb_function = MB_FUNC_READ_COILS,
            .mb_address = 0x3,
            .length = 0x4},
        .interval_ms = 0x5,
    };

    min_init_context(&min_ctx, 0);

    min_send_frame(&min_ctx, 0x31, (const uint8_t *)&payload, sizeof payload);

    uint8_t buffer[UART_FIFO_DEEP] = {0};
    uint8_t i = 0;
    while (true)
    {
        if (i++ == 0)
            gpio_xor_mask(1u << PICO_DEFAULT_LED_PIN);

        uint8_t count = hw_uart_read(&esp_uart, (uint8_t *)&buffer, UART_FIFO_DEEP);
        min_poll(&min_ctx, buffer, count);

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static void task_pio_uart_tx(void *arg)
{
    (void)arg;

    // FIXME: This task is just a placeholder

    uint8_t count = 10;
    uint8_t buffer[count];
    for (int i = 0; i < count - 1; i++)
    {
        buffer[i] = 'A' + i;
    }
    buffer[count - 1] = '\n';

    while (true)
    {
        for (size_t i = 0; pio_uarts[i] != NULL; i++)
        {
            pio_uart_write(pio_uarts[i], (uint8_t *)&buffer, count);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void task_pio_uart_rx(void *arg)
{
    (void)arg;

    uint8_t rx_buffer[PIO_BUFFER_SIZE];
    while (true)
    {
        for (size_t i = 0; pio_uarts[i] != NULL; i++)
        {
            size_t received_length = pio_uart_read(pio_uarts[i], (uint8_t *)&rx_buffer, PIO_BUFFER_SIZE);
            if (received_length > 0)
            {
                printf("%u Received %zu bytes:\n", i, received_length);
                for (size_t j = 0; j < received_length; j++)
                {
                    printf("%c ", rx_buffer[j]);
                }
                printf("\n");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
