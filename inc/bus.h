#ifndef BUS_H_
#define BUS_H_

#include "uart.h"
#include "messages.h"

//
// Defines
//

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

typedef struct
{
    uint8_t address;
    uint8_t function; // mb_function
    uint8_t *data;
    uint16_t data_size;
    uint16_t crc;
} mb_frame_t;

struct periodic_reads
{
    uint8_t slave;       // Modbus Slave address
    uint8_t function;    // Modbus function to use
    uint16_t address;    // Modbus first address to process
    uint16_t length;     // How many entries to process
    TickType_t last_run; // When this periodic read last run
};

struct bus_context
{
    struct pio_uart *pio_uart;
    uint32_t baudrate;
    uint8_t bus;
    uint16_t periodic_interval;
    uint8_t periodic_reads_size;
    struct periodic_reads periodic_reads[];
};

//
// Prototypes
//

void bus_init(struct bus_context *bus_cfg);

#endif // BUS_H_