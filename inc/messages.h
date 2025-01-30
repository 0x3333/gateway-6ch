#ifndef MESSAGES_H_
#define MESSAGES_H_

#include <stdint.h>
#include <stddef.h>
#include "binary.h"

// Used to identify messages using min_id
enum message_types
{
    MESSAGE_CONFIG_BUS = B00000001, // Configure the bus
    MESSAGE_READ = B00000100,       // Read address
    MESSAGE_WRITE = B00001000,      // Write address
};

//
// Handler definition
struct m_handler
{
    uint8_t message_id;
    void (*handler)(const void *);
};

// *** Packed structs have fields ordered to optimize alignment ***

//
// Base struct for Modbus Messages
struct m_base
{
    uint8_t slave;    // Modbus Slave address
    uint8_t function; // Modbus function to use
    uint16_t address; // Modbus first address to process
    uint16_t length;  // How many entries to process
} __attribute__((packed));

//
// Configure call to enable a Bus
struct m_config_bus
{
    uint32_t baudrate;           // Bus baudrate
    uint16_t periodic_interval;  // The interval between periodic reads
    uint8_t bus;                 // From 0 to 5
    uint8_t periodic_reads_len; // periodic_reads[] array size
    struct m_base periodic_reads[];
} __attribute__((packed));

//
// Issue a read
struct m_read
{
    struct m_base base; // Modbus base data
    uint8_t bus;        // From 0 to 5
} __attribute__((packed));

//
// Issue a write
struct m_write
{
    struct m_base base;  // Modbus base data
    uint8_t bus;         // From 0 to 5
    uint8_t data_length; // Data length
    uint8_t data[];      // First reference to the data
} __attribute__((packed));

//
// Message Handlers
void handle_m_config_bus(struct m_config_bus *msg);
void handle_m_read(struct m_read *msg);
void handle_m_write(struct m_write *msg);

//
// All possible messages
extern const struct m_handler m_handlers[3];

#endif // MESSAGES_H_