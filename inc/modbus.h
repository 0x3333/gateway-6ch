#ifndef MODBUS_H_
#define MODBUS_H_

#include <stdint.h>

enum mb_function
{
    MB_FUNC_READ_COILS = 0x01,
    MB_FUNC_WRITE_COILS = 0x0F,

    MB_FUNC_READ_HOLDING_REGISTERS = 0x03,
    MB_FUNC_WRITE_HOLDING_REGISTERS = 0x10,
};

// typedef struct
// {
//     uint8_t address;
//     uint8_t function;
//     uint8_t *data;
//     uint16_t data_size;
//     uint16_t crc;
// } mb_frame_t;

#endif // MODBUS_H_