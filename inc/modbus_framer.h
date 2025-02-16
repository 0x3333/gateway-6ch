#ifndef MODBUS_FRAMER_H
#define MODBUS_FRAMER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "modbus.h"
#include "macrologger.h"

// Creates a Read Coils (function 0x01) frame.
// Frame: [slave][0x01][startHi][startLo][quantityHi][quantityLo][CRClo][CRChi]
// Returns the frame length, or 0 if frameSize is too small.
static inline size_t modbus_create_read_coils_frame(uint8_t slave_address,
                                                    uint16_t start_address,
                                                    uint16_t quantity,
                                                    uint8_t *frame,
                                                    size_t frame_size)
{
    const size_t data_len = 6; // slave, function, start (2), quantity (2)
    const size_t total_len = data_len + 2;

    if (frame_size < total_len)
        return 0;

    size_t pos = 0;
    frame[pos++] = slave_address;
    frame[pos++] = 0x01;
    frame[pos++] = (uint8_t)(start_address >> 8);
    frame[pos++] = (uint8_t)(start_address & 0xFF);
    frame[pos++] = (uint8_t)(quantity >> 8);
    frame[pos++] = (uint8_t)(quantity & 0xFF);

    uint16_t crc = compute_crc(frame, data_len);
    frame[pos++] = (uint8_t)(crc & 0xFF);        // CRC low byte
    frame[pos++] = (uint8_t)((crc >> 8) & 0xFF); // CRC high byte

    return total_len;
}

// Creates a Read Holding Registers (function 0x03) frame.
// Frame: [slave][0x03][startHi][startLo][quantityHi][quantityLo][CRClo][CRChi]
static inline size_t modbus_create_read_holding_registers_frame(uint8_t slave_address,
                                                                uint16_t start_address,
                                                                uint16_t quantity,
                                                                uint8_t *frame,
                                                                size_t frame_size)
{
    const size_t data_len = 6;
    const size_t total_len = data_len + 2;

    if (frame_size < total_len)
        return 0;

    size_t pos = 0;
    frame[pos++] = slave_address;
    frame[pos++] = 0x03;
    frame[pos++] = (uint8_t)(start_address >> 8);
    frame[pos++] = (uint8_t)(start_address & 0xFF);
    frame[pos++] = (uint8_t)(quantity >> 8);
    frame[pos++] = (uint8_t)(quantity & 0xFF);

    uint16_t crc = compute_crc(frame, data_len);
    frame[pos++] = (uint8_t)(crc & 0xFF);
    frame[pos++] = (uint8_t)((crc >> 8) & 0xFF);

    return total_len;
}

// Creates a Write Single Coil (function 0x05) frame.
// 'on' true writes 0xFF00 (ON), false writes 0x0000 (OFF).
// Frame: [slave][0x05][coilAddrHi][coilAddrLo][valueHi][valueLo][CRClo][CRChi]
static inline size_t modbus_create_write_single_coil_frame(uint8_t slave_address,
                                                           uint16_t coil_address,
                                                           bool on,
                                                           uint8_t *frame,
                                                           size_t frame_size)
{
    const size_t data_len = 6;
    const size_t total_len = data_len + 2;

    if (frame_size < total_len)
        return 0;

    size_t pos = 0;
    frame[pos++] = slave_address;
    frame[pos++] = 0x05;
    frame[pos++] = (uint8_t)(coil_address >> 8);
    frame[pos++] = (uint8_t)(coil_address & 0xFF);

    // Per Modbus spec: ON = 0xFF00, OFF = 0x0000.
    uint16_t value = on ? 0xFF00 : 0x0000;
    frame[pos++] = (uint8_t)(value >> 8);
    frame[pos++] = (uint8_t)(value & 0xFF);

    uint16_t crc = compute_crc(frame, data_len);
    frame[pos++] = (uint8_t)(crc & 0xFF);
    frame[pos++] = (uint8_t)((crc >> 8) & 0xFF);

    return total_len;
}

// Creates a Write Multiple Registers (function 0x10) frame.
// 'registers' is an array of register values and 'quantity' is the number of registers.
// Frame: [slave][0x10][startHi][startLo][quantityHi][quantityLo][byteCount][reg1Hi][reg1Lo]... [CRClo][CRChi]
static inline size_t modbus_create_write_multiple_registers_frame(uint8_t slave_address,
                                                                  uint16_t start_address,
                                                                  const uint16_t *registers,
                                                                  size_t quantity,
                                                                  uint8_t *frame,
                                                                  size_t frame_size)
{
    if (quantity == 0 || quantity > 123)
        return 0;

    // Fixed bytes: slave (1) + function (1) + start (2) + quantity (2) + byteCount (1)
    // Each register: 2 bytes, plus 2 bytes for CRC.
    const size_t data_len = 1 + 1 + 2 + 2 + 1 + (quantity * 2);
    const size_t total_len = data_len + 2;

    if (frame_size < total_len)
        return 0;

    size_t pos = 0;
    frame[pos++] = slave_address;
    frame[pos++] = 0x10;
    frame[pos++] = (uint8_t)(start_address >> 8);
    frame[pos++] = (uint8_t)(start_address & 0xFF);
    frame[pos++] = (uint8_t)(quantity >> 8);
    frame[pos++] = (uint8_t)(quantity & 0xFF);
    // Byte count: 2 bytes per register.
    frame[pos++] = (uint8_t)(quantity * 2);

    for (size_t i = 0; i < quantity; ++i)
    {
        frame[pos++] = (uint8_t)(registers[i] >> 8);
        frame[pos++] = (uint8_t)(registers[i] & 0xFF);
    }

    uint16_t crc = compute_crc(frame, data_len);
    frame[pos++] = (uint8_t)(crc & 0xFF);
    frame[pos++] = (uint8_t)((crc >> 8) & 0xFF);

    return total_len;
}

//
// Create a Read Coils or Holding Registers frame based on parameters(Will only read 16 bits at a time)
static inline size_t modbus_create_read_frame(enum modbus_function function,
                                              uint8_t slave_address,
                                              uint16_t start_address,
                                              uint8_t *frame,
                                              size_t frame_size)
{
    if (function == MODBUS_FUNCTION_READ_COILS)
    {
        return modbus_create_read_coils_frame(slave_address, start_address, 16, frame, frame_size);
    }
    else if (function == MODBUS_FUNCTION_READ_HOLDING_REGISTERS)
    {
        return modbus_create_read_holding_registers_frame(slave_address, start_address, 1, frame, frame_size);
    }
    else
    {
        LOG_ERROR("Invalid Modbus function %u", function);
        return 0;
    }
}

//
// Create a Write Coils or Holding Registers frame based on parameters(Will only write 16 bits at a time)
static inline size_t modbus_create_write_frame(enum modbus_function function,
                                               uint8_t slave_address,
                                               uint16_t start_address,
                                               uint16_t value,
                                               uint8_t *frame,
                                               size_t frame_size)
{
    if (function == MODBUS_FUNCTION_WRITE_SINGLE_COIL)
    {
        return modbus_create_write_single_coil_frame(slave_address, start_address, value, frame, frame_size);
    }
    else if (function == MODBUS_FUNCTION_WRITE_HOLDING_REGISTERS)
    {
        return modbus_create_write_multiple_registers_frame(slave_address, start_address, &value, 1, frame, frame_size);
    }
    else
    {
        LOG_ERROR("Invalid Modbus function %u", function);
        return 0;
    }
}

#endif // MODBUS_FRAMER_H