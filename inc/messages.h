#ifndef MESSAGES_H_
#define MESSAGES_H_

#include <stdint.h>
#include <stddef.h>

// Used to identify messages using min_id
enum message_types
{
    MESSAGE_CONFIG_BUS = /*            */ 0x1,  // Configure the bus
    MESSAGE_READ = /*                  */ 0x2,  // Read address
    MESSAGE_READ_RESPONSE = /*         */ 0x3,  // A response for a read
    MESSAGE_WRITE = /*                 */ 0x4,  // Write address
    MESSAGE_WRITE_RESPONSE = /*        */ 0x5,  // A response for a write
    MESSAGE_PERIODIC_READ_RESPONSE = /**/ 0x6,  // A response for a periodic read
    MESSAGE_PICO_READY = /*            */ 0x7,  // When Pico resets, it sends this message to inform the host it is ready
    MESSAGE_PICO_RESET = /*            */ 0x8,  // Host send this to Pico to reset and get to a known state
    MESSAGE_HEARTBEAT = /*             */ 0x3F, // Sends this message to inform the device it is alive
};

//
// Handler definition
struct m_handler
{
    uint8_t message_id;
    void (*handler)(const void *);
};

// *** Structs have fields order to optimize alignment by hand, but they are packed ***

//
// Base struct for Modbus Messages
struct m_base
{
    uint8_t slave;    // Modbus Slave address
    uint8_t function; // Modbus function to use
    uint16_t address; // Modbus first address to process
} __attribute__((packed));

//
// Configure call to enable a Bus
struct m_config_bus
{
    uint32_t baudrate;             // Bus baudrate
    uint16_t periodic_interval;    // The interval between periodic reads
    uint8_t bus;                   // From 0 to 5
    uint8_t periodic_reads_length; // periodic_reads[] array size
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
// Read response
struct m_read_response
{
    struct m_base base; // Modbus base data
    uint16_t data;      // Data as 16 bits representation
    uint16_t data_mask; // Mask to identify which bits changed
    uint8_t bus;        // From 0 to 5
} __attribute__((packed));

//
// Issue a write
struct m_write
{
    struct m_base base; // Modbus base data
    uint16_t data;      // Data to write
    uint8_t bus;        // From 0 to 5
} __attribute__((packed));

//
// Write response
struct m_write_response
{
    struct m_base base; // Modbus base data
    uint8_t success;    // Success flag
    uint8_t bus;        // From 0 to 5
} __attribute__((packed));

//
// Message Handlers
void handle_m_config_bus(const struct m_config_bus *msg);
void handle_m_read(const struct m_read *msg);
void handle_m_write(const struct m_write *msg);
void handle_m_pico_reset(const uint8_t *msg);
void handle_m_heartbeat(const uint8_t *msg);

//
// All possible messages
extern const struct m_handler m_handlers[5];

#endif // MESSAGES_H_