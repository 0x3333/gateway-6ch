#ifndef HOST_H_
#define HOST_H_

#include <stdint.h>

#include "uart.h"

//
// Defines

#ifndef MB_SEND_WAIT_TIME
#define MB_SEND_WAIT_TIME 30
#endif

#ifndef HOST_UART
#define HOST_UART hw_uart1
#endif

//
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

// typedef struct
// {
//     uint8_t address;
//     uint8_t function;
//     uint8_t *data;
//     uint16_t data_size;
//     uint16_t crc;
// } mb_frame_t;

void init_host(void);

#endif // HOST_H_