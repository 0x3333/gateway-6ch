#include <stdio.h>
#include <pico/stdlib.h>
#include <pico/multicore.h>

#include <FreeRTOS.h>
#include <task.h>
#include <target/min.h>

#include "debug.h"

#include "uart.h"
#include "led.h"
#include "host.h"
#include "messages.h"
#include "esp.h"
#include "res_usage.h"

int main()
{
    // Allow core1 to launch when debugger is atached
    timer_hw->dbgpause = 0;
    multicore_reset_core1();

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

    // Init Tasks
    printf("Creating tasks...\n");

    init_uart_maintenance();
    init_led();
    init_host();
    init_res_usage();

    printf("Running\n\n");
    vTaskStartScheduler();

    while (true)
        tight_loop_contents();
}

// static void task_comm_esp(void *arg)
// {
//     (void)arg;

//     gpio_init(PICO_DEFAULT_LED_PIN);
//     gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

//     // uint8_t data[UARTS_BUFFER_SIZE];
//     // for (size_t i = 0; i < UARTS_BUFFER_SIZE; i++)
//     // {
//     //     data[i] = i + 1;
//     // }
//     // hw_uart_write_bytes(&esp_uart, data, UARTS_BUFFER_SIZE);

//     struct p_config_mb_read payload = {
//         .base = {
//             .bus = 1,
//             .slave = 0x2,
//             .function = MB_FUNC_READ_COILS,
//             .address = 0x3,
//             .length = 0x4},
//         .interval_ms = 0x5,
//     };

//     min_init_context(&min_ctx, 0);

//     min_send_frame(&min_ctx, 0x31, (const uint8_t *)&payload, sizeof payload);

//     uint8_t buffer[UARTS_BUFFER_SIZE / 2] = {0};
//     uint8_t i = 0;
//     while (true)
//     {
//         if (i++ == 0)
//             gpio_xor_mask(1u << PICO_DEFAULT_LED_PIN);

//         uint8_t count = hw_uart_read_bytes(&esp_uart, &buffer, UARTS_BUFFER_SIZE / 2);
//         min_poll(&min_ctx, buffer, count);

//         vTaskDelay(pdMS_TO_TICKS(1));
//     }
// }

// static void task_pio_uart_tx(void *arg)
// {
//     (void)arg;

//     // FIXME: This task is just a placeholder

//     uint8_t count = 64;
//     uint8_t buffer[count];
//     for (int i = 0; i < count; i++)
//     {
//         buffer[i] = 0x21 + i;
//     }

//     while (true)
//     {
//         for (size_t i = 0; pio_uarts[i] != NULL; i++)
//         {
//             pio_uart_write_bytes(pio_uarts[i], &buffer, count);
//         }

//         vTaskDelay(pdMS_TO_TICKS(1000));
//     }
// }

// static void task_pio_uart_rx(void *arg)
// {
//     (void)arg;

//     uint8_t rx_buffer[UARTS_BUFFER_SIZE];
//     while (true)
//     {
//         for (size_t i = 0; pio_uarts[i] != NULL; i++)
//         {
//             size_t received_length = pio_uart_read_bytes(pio_uarts[i], &rx_buffer, UARTS_BUFFER_SIZE);
//             if (received_length > 0)
//             {
//                 printf("%u Received %zu bytes:\n", i, received_length);
//                 for (size_t j = 0; j < received_length; j++)
//                 {
//                     printf("%c ", rx_buffer[j]);
//                 }
//                 printf("\n");
//             }
//         }
//         vTaskDelay(pdMS_TO_TICKS(1));
//     }
// }
