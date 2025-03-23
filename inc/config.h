#ifndef __CONFIG_H__
#define __CONFIG_H__

//
// Task Configuration
//

#define HOST_TASK_CORE_AFFINITY 0x01
#define BUS_TASK_CORE_AFFINITY 0x02
#define UART_ACTIVITY_TASK_CORE_AFFINITY 0x01
#define UART_MAINTENANCE_TASK_CORE_AFFINITY 0x01
#define RESOURCE_USAGE_TASK_CORE_AFFINITY 0x02

//
// Bus Configuration
//

#define BUS_MODBUS_FRAME_BUFFER_SIZE 64

// After how many ms we will print the timeout message again
#define BUS_DELAY_TIMEOUT_MSG 5000
// After how many ms we will consider the modbus command has timed out
#define BUS_TIMEOUT_RESPONSE 20
// How many ms we will wait after writing to UART before reading
#define BUS_DELAY_WRITE_READ 3
// How many ms we will wait until we start the bus after configured
#define BUS_START_DELAY 300
// #define BUS_DEBUG_MODBUS_FRAME
// #define BUS_DEBUG_PERIODIC_READS

//
// Host Configuration
//

#define HOST_UART hw_uart1
#define HOST_QUEUE_LENGTH 200
#define HOST_HEARTBEAT_INTERVAL 1000
// #define HOST_DEBUG_MIN_FRAME

//
// UART Configuration
//

#define UART_BUFFER_SIZE 1024

// Max number of PIO UARTs possible
#define COUNT_HW_UARTS NUM_UARTS
#define COUNT_PIO_UARTS 5u
#define MAX_PIO_UARTS (NUM_PIOS * NUM_PIO_STATE_MACHINES / 2u)

/*
 * PIO UART
 *
 * We use 2 PIO state machines per UART channel.
 * By default, RX channel is located in PIO0(PIO_UART_RX_PIO), TX channel in PIO1(PIO_UART_TX_PIO).
 */

#define PIO_UART_RX_PIO pio0
#define PIO_UART_TX_PIO pio1

#define PIO_UART_RX_FIFO_IRQ_INDEX 0
#define PIO_UART_TX_DONE_IRQ_INDEX 0
#define PIO_UART_TX_FIFO_IRQ_INDEX 1

#if PIO_UART_TX_DONE_IRQ_INDEX == PIO_UART_TX_FIFO_IRQ_INDEX
#error PIO_UART_TX_DONE_IRQ_INDEX cannot be equal PIO_UART_TX_FIFO_IRQ_INDEX
#endif
#if PIO_UART_TX_FIFO_IRQ_INDEX == PIO_UART_RX_FIFO_IRQ_INDEX
#error PIO_UART_TX_FIFO_IRQ_INDEX cannot be equal PIO_UART_RX_FIFO_IRQ_INDEX
#endif

#define HW_UART_DEFAULT_BAUDRATE 230400
#define PIO_UART_DEFAULT_BAUDRATE 115200

//
// DMX Configuration
//

#define DMX_PIO pio0
#define DMX_BAUDRATE 250000
#define DMX_MAX_CHANNELS 12
#define DMX_DELAY_BETWEEN_WRITES ((TickType_t)(1000 / 12)) // 12Hz
#define DMX_WRITE_QUEUE_LENGTH 100
//
// LED Configuration
//

#define LED_PIN_ACT 0
#define LED_PIN_BUILTIN PICO_DEFAULT_LED_PIN

#define LED_ACT_DELAY 40

#define LED_BUILTIN_DELAY 250

//
// Resource Usage Configuration
//

#define RES_USAGE_STATS_DELAY 5000
#define RES_USAGE_TASKS_ARRAY_SIZE 20

//
// Utils
//

// Size of the input bytes, not the internal buffer.
#define TO_HEX_STRING_MAX_SIZE 50
#define TO_BIN_HEX_STRING_MAX_SIZE 6

#endif // __CONFIG_H__