# ETH <-> RS485 Gateway

This repo contains the code for a custom PCB with ESP32(WT32-ETH01) + Pico(rp2040) and 6 channels RS-485.

The software stack is the Pico SDK + FreeRTOS.

## Debug

When compiling for Debug, it will disable SMP, so we can use the Debugger(PicoProbe or BlackMagic Debug).

More to come.
