#include <stdio.h>
#include <pico/stdlib.h>
#include <pico/multicore.h>

#include <FreeRTOS.h>
#include <task.h>
#include <target/min.h>
#include "macrologger.h"

#include "uart.h"
#include "led.h"
#include "host.h"
#include "dmx.h"
#include "messages.h"
#include "res_usage.h"

int main()
{
    // Disable IO that is wrongly connected to the ESP
    gpio_init(2);
    gpio_set_dir(2, GPIO_IN);
    gpio_init(3);
    gpio_set_dir(3, GPIO_IN);

    // Allow core1 to launch when debugger is atached
    timer_hw->dbgpause = 0;
    multicore_reset_core1();

    stdio_init_all();

#if LIB_PICO_STDIO_USB
    // Wait for 1 seconds USB CDC to connect, otherwise, continue
    uint_fast8_t wait = 0;
    while (!stdio_usb_connected())
    {
        wait++;
        if (wait >= 1000 / 8)
            break;
        sleep_ms(8);
    }
    LOG_INFO("Wait: %u ms", wait * 8);
#endif

    // Header
    LOG_INFO("   ___   ____ ____  ___   ____   _____       __                         ");
    LOG_INFO("  / _ \\ / __// / / ( _ ) / __/  / ___/___ _ / /_ ___  _    __ ___ _ __ __");
    LOG_INFO(" / , _/_\\ \\ /_  _// _  |/__ \\  / (_ // _ `// __// -_)| |/|/ // _ `// // /");
    LOG_INFO("/_/|_|/___/  /_/  \\___//____/  \\___/ \\_,_/ \\__/ \\__/ |__,__/ \\_,_/ \\_, / ");
    LOG_INFO("                                                                  /___/  ");
    LOG_INFO("By: Tercio Gaudencio Filho - %s %s\n", __DATE__, __TIME__);

    // TODO: Print some configurations like Host Baudrate, queue lengths, heartbeat interval, etc.

    // Init Peripherals
    LOG_INFO("Initializing Peripherals");

    // Init Tasks
    LOG_INFO("Creating tasks");

    uart_maintenance_init();
    led_init();
    dmx_init();
    host_init();
    res_usage_init();

    LOG_INFO("Running\n");
    vTaskStartScheduler();

    for (;;) // Task infinite loop
        tight_loop_contents();
}
