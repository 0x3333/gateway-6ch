#ifndef MESSAGES_H_
#define MESSAGES_H_

#include <stdint.h>
#include <stddef.h>

// Used to identify messages using min_id
enum message_types
{
    MESSAGE_CONFIG_BUS_ID = /**/ 0x1 << 0, // Configure the bus
    MESSAGE_READ_ID = /*      */ 0x1 << 1, // Read address
    MESSAGE_WRITE_ID = /*     */ 0x1 << 2, // Write address
    MESSAGE_CHANGE_ID = /*    */ 0x1 << 3, // A changed has been detected
    MESSAGE_PICO_READY_ID = /**/ 0x1 << 4, // When Pico resets, it sends this message to inform the host it is ready
    MESSAGE_PICO_RESET_ID = /**/ 0x1 << 5, // Host send this to Pico to reset and get to a known state
    MESSAGE_HEARTBEAT_ID = /* */ 0x3F,     // Sends this message to inform the device it is alive
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
    uint16_t length;  // How many entries to process
} __attribute__((packed));

//
// Configure call to enable a Bus
struct m_config_bus
{
    uint32_t baudrate;          // Bus baudrate
    uint16_t periodic_interval; // The interval between periodic reads
    uint8_t bus;                // From 0 to 5
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
// Modbus change
struct m_change
{
    struct m_base base; // Modbus base data
    uint8_t bus;        // From 0 to 5
    uint16_t data;      // First reference to the data
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