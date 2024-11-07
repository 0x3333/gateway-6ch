#ifndef MESSAGES_H_
#define MESSAGES_H_

#include <stdint.h>
#include "binary.h"

// Used to identify messages using min_id
enum message_types
{
    MESSAGE_CONFIG_MB_BUS = B00000001,  // Configure to read address constantly
    MESSAGE_CONFIG_MB_READ = B00000010, // Configure to read address constantly
    MESSAGE_MB_READ = B00000100,        // Read address
    MESSAGE_MB_WRITE = B00001000,       // Write address
};

//
// Configure call to enable a Bus
struct m_config_bus
{
    uint8_t bus;
    uint8_t _reserved[3];
    uint32_t baudrate;
} __attribute__((packed));

//
// Base struct for Modbus Messages
struct m_mb_base
{
    uint8_t bus;       // From 0 to 5
    uint8_t slave;     // Modbus Slave address
    uint8_t function;  // Modbus function to use
    uint8_t _reserved; // Alignment padding
    uint16_t address;  // Modbus first address to process
    uint16_t length;   // How many entries to process
} __attribute__((packed));

//
// Configure periodic reads
struct m_config_mb_read
{
    struct m_mb_base base; // Modbus base data
    uint16_t interval_ms;  // Interval in ms between reads
} __attribute__((packed));

//
// Issue a read
struct m_mb_read
{
    struct m_mb_base base; // Modbus base data
} __attribute__((packed));

//
// Issue a write
struct m_mb_write
{
    struct m_mb_base base; // Modbus base data
    uint8_t data_length;   // Data length
    uint8_t data[];        // First reference to the data
} __attribute__((packed));

#endif // MESSAGES_H_