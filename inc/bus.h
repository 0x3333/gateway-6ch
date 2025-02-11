#ifndef BUS_H_
#define BUS_H_

#include <FreeRTOS.h>
#include "uart.h"
#include "messages.h"
#include "modbus.h"
#include "modbus_framer.h"
#include "modbus_parser.h"

//
// Defines
//

#ifndef BUS_TIMEOUT
#define BUS_TIMEOUT 50
#endif

#ifndef BUS_MODBUS_FRAME_BUFFER_SIZE
#define BUS_MODBUS_FRAME_BUFFER_SIZE 64
#endif

//
// Data Structures
//

struct bus_periodic_read
{
    uint8_t slave;          // Modbus Slave address
    uint8_t function;       // Modbus function to use
    uint16_t address;       // Modbus first address to process
    uint16_t length;        // How many entries to process
    TickType_t last_run;    // When this periodic read last run
    size_t memory_map_size; // Memory map size
    uint8_t *memory_map;    // Memory map that holds the values
};

struct bus_context
{
    struct pio_uart *pio_uart;
    uint32_t baudrate;
    uint8_t bus;
    uint16_t periodic_interval;
    uint8_t periodic_reads_len;
    struct bus_periodic_read periodic_reads[];
};

//
// Prototypes
//

void bus_init(struct bus_context *bus_cfg);

struct bus_context *bus_get_context(uint8_t bus);

void process_modbus_response(uint8_t bus, struct bus_periodic_read *p_read, struct modbus_frame *frame);

#endif // BUS_H_