#ifndef MODBUS_RTU_PARSER_H
#define MODBUS_RTU_PARSER_H

#include <stdint.h>
#include <stddef.h>
#include "modbus.h"

// Frame structure to store Modbus frame data
struct modbus_frame
{
    uint8_t slave;
    uint8_t function_code;
    uint8_t address;
    uint8_t data[8];
    uint8_t data_size;
    uint16_t crc;
};

// Parser state enum
enum modbus_parser_state
{
    WAIT_SLAVE = 0,
    WAIT_FUNCTION,
    WAIT_ADDRESS_1,
    WAIT_ADDRESS_2,
    WAIT_LENGTH,
    WAIT_DATA,
    WAIT_CRC1,
    WAIT_CRC2
};

// Result enum
enum modbus_result
{
    MODBUS_COMPLETE = 0,
    MODBUS_ERROR = 1,
    MODBUS_INCOMPLETE = 2
};

// Parser context structure
struct modbus_parser
{
    enum modbus_parser_state state;
    uint16_t crc;
    size_t data_length;
};

// Function to add data to frame
static inline void modbus_frame_add_data(struct modbus_frame *frame, uint8_t byte)
{
    frame->data[frame->data_size++] = byte;
}

// Reset parser
static inline void modbus_parser_reset(struct modbus_parser *parser)
{
    parser->state = WAIT_SLAVE;
    parser->crc = 0xFFFF;
    parser->data_length = 0;
}

static bool is_valid_modbus_function(uint8_t functionCode)
{
    return (functionCode == 0x01 || // Read Coils
            functionCode == 0x03 || // Read Holding Registers
            functionCode == 0x05 || // Write Single Coil
            functionCode == 0x06 || // Write Single Register
            functionCode == 0x10);  // Write Multiple Registers
}

// Process a single byte
static inline enum modbus_result modbus_parser_process_byte(struct modbus_parser *parser,
                                                            struct modbus_frame *frame,
                                                            uint8_t byte)
{
    enum modbus_result ret = MODBUS_INCOMPLETE;

    switch (parser->state)
    {
    case WAIT_SLAVE:
        frame->slave = byte;
        if (byte > 247)
        {
            ret = MODBUS_ERROR;
            modbus_parser_reset(parser);
        }
        else
        {
            update_crc(&parser->crc, byte);
            parser->state = WAIT_FUNCTION;
        }
        break;

    case WAIT_FUNCTION:
        frame->function_code = byte;
        if (!is_valid_modbus_function(byte))
        {
            ret = MODBUS_ERROR;
            modbus_parser_reset(parser);
        }
        else
        {
            update_crc(&parser->crc, byte);
            frame->data_size = 0;
            switch (frame->function_code)
            {
            case 0x01:
            case 0x02:
            case 0x03:
            case 0x04:
                parser->state = WAIT_LENGTH;
                break;

            case 0x05:
            case 0x06:
            case 0x0F:
            case 0x10:
                parser->state = WAIT_ADDRESS_1;
                break;
            }
        }
        break;

    case WAIT_ADDRESS_1:
        update_crc(&parser->crc, byte);
        frame->address = byte;
        parser->state = WAIT_ADDRESS_2;
        break;

    case WAIT_ADDRESS_2:
        update_crc(&parser->crc, byte);
        frame->address |= (byte << 8);
        parser->data_length = 2;
        parser->state = WAIT_DATA;

        break;

    case WAIT_LENGTH:
        update_crc(&parser->crc, byte);
        parser->data_length = byte;
        frame->data_size = 0;
        parser->state = WAIT_DATA;
        break;

    case WAIT_DATA:
        update_crc(&parser->crc, byte);
        modbus_frame_add_data(frame, byte);
        if (frame->data_size == parser->data_length)
        {
            parser->state = WAIT_CRC1;
        }
        break;

    case WAIT_CRC1:
        frame->crc = byte;
        parser->state = WAIT_CRC2;
        break;

    case WAIT_CRC2:
        frame->crc |= (byte << 8);
        ret = (parser->crc == frame->crc) ? MODBUS_COMPLETE : MODBUS_ERROR;
        modbus_parser_reset(parser);
        break;
    }

    return ret;
}

#endif // MODBUS_RTU_PARSER_H