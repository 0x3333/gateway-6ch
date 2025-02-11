#ifndef MODBUS_RTU_PARSER_H
#define MODBUS_RTU_PARSER_H

#include <stdint.h>
#include <stddef.h>
#include "modbus.h"

//
// Usage
//

// struct ModbusParser parser;
// struct ModbusFrame frame;

// modbus_parser_init(&parser);

// // Process bytes as they arrive
// enum ModbusResult result = modbus_parser_process_byte(&parser, incoming_byte, &frame);
// if (result == MODBUS_COMPLETE) {
//     // Frame is complete and valid
// } else if (result == MODBUS_ERROR) {
//     // Frame had a CRC error
// }

// Frame structure to store Modbus frame data
struct ModbusFrame
{
    uint8_t address;
    uint8_t function_code;
    uint8_t data[32];
    uint8_t data_size;
    uint16_t crc;
};

// Parser state enum
enum ModbusParserState
{
    WAIT_ADDRESS,
    WAIT_FUNCTION,
    WAIT_LENGTH,
    WAIT_DATA,
    WAIT_CRC1,
    WAIT_CRC2
};

// Result enum
enum ModbusResult
{
    MODBUS_ERROR,
    MODBUS_COMPLETE,
    MODBUS_INCOMPLETE
};

// Parser context structure
struct ModbusParser
{
    enum ModbusParserState state;
    uint16_t crc;
    size_t data_length;
};

// Function to add data to frame
static inline void modbus_frame_add_data(struct ModbusFrame *frame, uint8_t byte)
{
    frame->data[frame->data_size++] = byte;
}

// Reset parser
static inline void modbus_parser_reset(struct ModbusParser *parser)
{
    parser->state = WAIT_ADDRESS;
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
static inline enum ModbusResult modbus_parser_process_byte(struct ModbusParser *parser,
                                                           struct ModbusFrame *frame,
                                                           uint8_t byte)
{
    enum ModbusResult ret = MODBUS_INCOMPLETE;

    switch (parser->state)
    {
    case WAIT_ADDRESS:
        frame->address = byte;
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
            parser->state = WAIT_LENGTH;
        }
        break;

    case WAIT_LENGTH:
        parser->data_length = byte;
        frame->data_size = 0;
        update_crc(&parser->crc, byte);
        parser->state = WAIT_DATA;
        break;

    case WAIT_DATA:
        modbus_frame_add_data(frame, byte);
        update_crc(&parser->crc, byte);
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