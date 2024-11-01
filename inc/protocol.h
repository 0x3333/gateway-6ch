#ifndef PROTOCOL_H_
#define PROTOCOL_H_

#include <stdint.h>

// Which kind of functions exists
enum p_function
{
    FUNCTION_CONFIG_MB_BUS = 0x01,  // Configure to read address constantly
    FUNCTION_CONFIG_MB_READ = 0x02, // Configure to read address constantly
    FUNCTION_MB_READ = 0x04,        // Read address
    FUNCTION_MB_WRITE = 0x8,        // Write address
};

// Configure call to enable a Bus
struct p_config_mb_bus
{
    uint8_t bus; // From 1 to 6
} __attribute__((packed));

// Base struct for Modbus
struct p_base
{
    uint8_t bus;         // From 0 to 5
    uint8_t mb_slave;    // Modbus Slave address
    uint8_t mb_function; // Modbus function to use
    uint8_t _padding;    // Alignment padding
    uint16_t mb_address; // Modbus first address to process
    uint16_t length;     // How many entries to process
} __attribute__((packed));

// Configure call for periodically reads
struct p_config_mb_read
{
    struct p_base base;   // Modbus base data
    uint16_t interval_ms; // Interval in ms between reads
} __attribute__((packed));

// Issue a read
struct p_mb_read
{
    struct p_base base; // Modbus base data
    // FIXME: Add a live or can be from the local buffer values
} __attribute__((packed));

// Issue a write
struct p_mb_write
{
    struct p_base base; // Modbus base data
    uint16_t data;      // Data to write
} __attribute__((packed));

#endif // PROTOCOL_H_