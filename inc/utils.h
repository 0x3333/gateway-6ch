#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdlib.h>
#include <FreeRTOS.h>
#include <task.h>

#include "config.h"

//
// Defines
//

// FreeRTOS Tick helper functions
#define NEXT_TIMEOUT(timeout) (xTaskGetTickCount() + pdMS_TO_TICKS(timeout))
#define IS_EXPIRED(timeout) (xTaskGetTickCount() >= timeout)

#define QUEUE_NO_WAIT (0)

//
// Functions
//

/**
 * @brief Convert a byte array to a hex string.
 * The returned string is static and should not be freed or kept.
 */
static inline const char *to_hex_string(const uint8_t *data, uint8_t len)
{
    static char hex_str[TO_HEX_STRING_MAX_SIZE * 3 + 1];
    len = MIN(len, TO_HEX_STRING_MAX_SIZE);
    for (uint8_t i = 0; i < len; i++)
    {
        snprintf(&hex_str[i * 3], 4, "%02X ", data[i]);
    }
    if (len > TO_HEX_STRING_MAX_SIZE)
    {
        hex_str[sizeof(hex_str) - 5] = '.';
        hex_str[sizeof(hex_str) - 4] = '.';
        hex_str[sizeof(hex_str) - 3] = '.';
        hex_str[sizeof(hex_str) - 1] = '\0';
    }
    return hex_str;
}

/**
 * @brief Convert a byte array to a binary string.
 * The returned string is static and should not be freed or kept.
 */
static inline const char *to_bin_hex_string(const uint8_t *data, uint8_t len)
{
    static char hex_str[TO_BIN_HEX_STRING_MAX_SIZE * 9 + 1];
    uint8_t max_len = MIN(len, TO_BIN_HEX_STRING_MAX_SIZE);

    for (uint8_t i = 0; i < max_len; i++)
    {
        for (uint8_t j = 0; j < 8; j++)
        {
            hex_str[i * 9 + j] = (data[i] & (1 << (7 - j))) ? '1' : '0';
        }
        hex_str[i * 9 + 8] = ' ';
    }
    if (len > TO_BIN_HEX_STRING_MAX_SIZE)
    {
        hex_str[sizeof(hex_str) - 4] = '.';
        hex_str[sizeof(hex_str) - 3] = '.';
        hex_str[sizeof(hex_str) - 2] = '.';
    }
    hex_str[max_len * 9] = '\0';
    return hex_str;
}

#endif // __UTILS_H__