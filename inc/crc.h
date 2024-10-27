#ifndef CRC_H_
#define CRC_H_

#include <stddef.h>
#include <stdint.h>

/**
 * Calculate a CRC16 of a buffer.
 */
uint16_t crc16(const uint8_t *buffer, size_t length);

#endif // CRC_H_