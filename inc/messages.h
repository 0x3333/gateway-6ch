#ifndef MESSAGES_H_
#define MESSAGES_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Used to identify messages using min_id
enum message_types
{
    MESSAGE_CONFIG_BUS = /*            */ 0x1,
    MESSAGE_CONFIG_BUS_REPLY = /*      */ 0x2,

    MESSAGE_PERIODIC_READ_REPLY = /*   */ 0x4, // When a change is detected in a periodic read

    MESSAGE_COMMAND_READ = /*          */ 0x8,
    MESSAGE_COMMAND_READ_REPLY = /*    */ 0x9,
    MESSAGE_COMMAND_WRITE = /*         */ 0xA,
    MESSAGE_COMMAND_WRITE_REPLY = /*   */ 0xB,

    MESSAGE_PICO_READY = /*            */ 0x3D,
    MESSAGE_PICO_RESET = /*            */ 0x3E,
    MESSAGE_HEARTBEAT = /*             */ 0x3F,
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
// Base struct for Modbus Device Messages
struct m_device
{
    uint8_t bus;      // From 0 to 5
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
    struct m_device periodic_reads[];
} __attribute__((packed));

//
// Issue a Read/Write Command
struct m_command
{
    uint8_t type;
    uint8_t seq;
    struct m_device device;
    union
    {
        // MESSAGE_COMMAND_READ and MESSAGE_COMMAND_TIMEOUT has no additional data
        struct
        {
            uint8_t bus;             // From 0 to 5
            bool done;               // If it was successful
            bool already_configured; // If the bus was already configured
            bool invalid_bus;        // If the bus number is invalid
        } __attribute__((packed)) config_bus_reply;
        struct
        {
            uint16_t data;      // Data as 16 bits representation
            uint16_t data_mask; // Mask to identify which bits changed
        } __attribute__((packed)) periodic_change;
        struct
        {
            bool done;     // If it was successful
            uint16_t data; // Data as 16 bits representation
        } __attribute__((packed)) read_reply;
        struct
        {
            uint16_t data; // Data to write
        } __attribute__((packed)) write;
        struct
        {
            bool done; // If it was successful
        } __attribute__((packed)) write_reply;
    } msg;
} __attribute__((packed));

//
// Message Handlers
void handle_m_config_bus(const struct m_config_bus *msg);
void handle_m_command(const struct m_command *msg);
void handle_m_pico_reset(const uint8_t *msg);

//
// Message handlers array
extern const struct m_handler m_handlers[4];

#endif // MESSAGES_H_