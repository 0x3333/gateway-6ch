#include <FreeRTOS.h>
#include <task.h>

#include "led.h"
#include "uart.h"

#ifndef LED_ACT_DELAY
#define LED_ACT_DELAY 40
#endif

void task_led(void *arg)
{
    (void)arg;

    gpio_init(LED_PIN_ACT);
    gpio_set_dir(LED_PIN_ACT, GPIO_OUT);

    gpio_init(LED_PIN_BUILDIN);
    gpio_set_dir(LED_PIN_BUILDIN, GPIO_OUT);

    gpio_xor_mask(1u << LED_PIN_BUILDIN);
    while (true)
    {
        // Act LED
        if (uart_activity)
        {
            uart_activity = false;

            gpio_xor_mask(1u << LED_PIN_ACT);
            gpio_xor_mask(1u << LED_PIN_BUILDIN);
            vTaskDelay(pdMS_TO_TICKS(LED_ACT_DELAY / 2));

            gpio_xor_mask(1u << LED_PIN_ACT);
            gpio_xor_mask(1u << LED_PIN_BUILDIN);
            vTaskDelay(pdMS_TO_TICKS(LED_ACT_DELAY / 2));
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(LED_ACT_DELAY));
        }
    }
}

void led_init(void)
{
    xTaskCreate(task_led, "LED", configMINIMAL_STACK_SIZE, NULL, tskLOW_PRIORITY, NULL);
}