#include <stdio.h>
#include "pico/stdlib.h"

#include "debug.h"

#include <FreeRTOS.h>
#include <task.h>

#include "uart.h"
#include "modbus.h"
#include "protocol.h"
#include "esp.h"
#include "target/min.h"

void task_uart_tx(void *arg);
void task_uart_rx(void *arg);
static void task_comm_esp(void *arg);

int main()
{
    stdio_init_all();

    uint_fast8_t wait = 0;
    while (!stdio_usb_connected())
    {
        wait++;
        if (wait >= 8)
            break;
        sleep_ms(250);
    }
    printf("\n");
    printf("   ___   ____ ____  ___   ____   _____       __                         \n");
    printf("  / _ \\ / __// / / ( _ ) / __/  / ___/___ _ / /_ ___  _    __ ___ _ __ __\n");
    printf(" / , _/_\\ \\ /_  _// _  |/__ \\  / (_ // _ `// __// -_)| |/|/ // _ `// // /\n");
    printf("/_/|_|/___/  /_/  \\___//____/  \\___/ \\_,_/ \\__/ \\__/ |__,__/ \\_,_/ \\_, / \n");
    printf("                                                                  /___/  \n");
    printf("By: Tercio Gaudencio Filho - %s %s\n\n", __DATE__, __TIME__);
    printf("Initializing...\n");

    init_uarts(1, &esp_uart);

    init_pio_uart(&pio_uart_bus_3);
    init_pio_uart(&pio_uart_bus_4);
    init_pio_uart(&pio_uart_bus_5);
    init_pio_uart(&pio_uart_bus_6);

    printf("Creating tasks...\n");
    xTaskCreate(task_comm_esp, "Comm-ESP", 1024, NULL, configDEFAULT_PRIORITY, NULL);
    xTaskCreate(task_uart_tx, "UART-TX", 2048, NULL, configDEFAULT_PRIORITY, NULL);
    xTaskCreate(task_uart_rx, "UART-RX", 2048, NULL, configDEFAULT_PRIORITY, NULL);

    printf("Running\n");

    vTaskStartScheduler();

    while (true)
        tight_loop_contents();
}

static void task_comm_esp(void *arg)
{
    (void)arg;

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    uart_inst_t *uart = esp_uart.uart;
    uint8_t c;
    while (true)
    {
        gpio_xor_mask(1u << PICO_DEFAULT_LED_PIN);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void task_uart_tx(void *arg)
{
    (void)arg;

    // FIXME: This task is just a placeholder

    gpio_init(0);
    gpio_set_dir(0, GPIO_OUT);

    while (true)
    {
        gpio_xor_mask(1u << 0);

        for (size_t i = 0; pio_uarts[i] != NULL; i++)
        {
            // Update TX buffer to some data
            for (int j = 0; j < PIO_TX_BUFFER_SIZE - 1; j++)
            {
                pio_uarts[i]->tx_dma_buffer[j] = 'A' + j;
            }
            pio_uarts[i]->tx_dma_buffer[PIO_TX_BUFFER_SIZE - 1] = '\n';

            if (!pio_uarts[i]->is_transmitting)
            {
                pio_tx_dma_start_transfer(pio_uarts[i], PIO_TX_BUFFER_SIZE);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void task_uart_rx(void *arg)
{
    (void)arg;

    uint8_t rx_buffer[PIO_RX_BUFFER_SIZE];
    while (true)
    {
        for (size_t i = 0; pio_uarts[i] != NULL; i++)
        {
            size_t received_length = xStreamBufferReceive(pio_uarts[i]->rx_sbuffer, &rx_buffer, sizeof rx_buffer, 0);
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
