#ifndef BUS_H_
#define BUS_H_

#include "uart.h"
#include "messages.h"

//
// Defines
//

#ifndef BUS_TIMEOUT
#define BUS_TIMEOUT 50
#endif

//
// Data Structures
//

enum mb_return
{
    MB_ERROR = -1,
    MB_TIMEOUT = -2,
};

enum mb_function
{
    MB_FUNC_READ_COILS = 0x01,
    MB_FUNC_WRITE_COILS = 0x0F,

    MB_FUNC_READ_HOLDING_REGISTERS = 0x03,
    MB_FUNC_WRITE_HOLDING_REGISTERS = 0x10,
};

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

/**
 * Convert a number of bits to it's byte size
 */
size_t bits_to_bytes(size_t bits);

/**
 * Get the array size for a Modbus function with len size.
 */
size_t bus_mb_function_to_map_size(enum mb_function func, size_t len);

#endif // BUS_H_