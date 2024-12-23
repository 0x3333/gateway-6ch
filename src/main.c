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
