#include <FreeRTOS.h>
#include <task.h>
#include "macrologger.h"

#include "led.h"
#include "uart.h"
#include "utils.h"

_Noreturn void task_led_act(void *arg)
{
    (void)arg;

    gpio_init(LED_PIN_ACT);
    gpio_set_dir(LED_PIN_ACT, GPIO_OUT);

    for (;;) // Task infinite loop
    {
        // Act LED
        if (uart_activity)
        {
            uart_activity = false;

            gpio_xor_mask(1u << LED_PIN_ACT);
            vTaskDelay(pdMS_TO_TICKS(LED_ACT_DELAY / 2));

            gpio_xor_mask(1u << LED_PIN_ACT);
            vTaskDelay(pdMS_TO_TICKS(LED_ACT_DELAY / 2));
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(LED_ACT_DELAY));
        }
    }
}

_Noreturn void task_led_builtin(void *arg)
{
    (void)arg;

    gpio_init(LED_PIN_BUILTIN);
    gpio_set_dir(LED_PIN_BUILTIN, GPIO_OUT);

    for (;;) // Task infinite loop
    {
        gpio_xor_mask(1u << LED_PIN_BUILTIN);
        vTaskDelay(pdMS_TO_TICKS(LED_BUILTIN_DELAY / 2));
    }
}

void led_init(void)
{
    LOG_DEBUG("Initializing LED tasks");

    xTaskCreateAffinitySet(task_led_act,
                           "LED Act",
                           configMINIMAL_STACK_SIZE,
                           NULL,
                           tskLOW_PRIORITY,
                           UART_ACTIVITY_TASK_CORE_AFFINITY,
                           NULL);

    xTaskCreateAffinitySet(task_led_builtin,
                           "LED Builtin",
                           configMINIMAL_STACK_SIZE,
                           NULL,
                           tskLOW_PRIORITY,
                           UART_MAINTENANCE_TASK_CORE_AFFINITY,
                           NULL);
}