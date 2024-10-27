#include <FreeRTOS.h>
#include <queue.h>

#include "esp.h"
#include "protocol.h"

//
// Type

// Used to access the payload as a single buffer
union payload
{
    struct p_config_mb_bus config_mb_bus;
    struct p_config_mb_read config_mb_read;
    struct p_mb_read mb_read;
    struct p_mb_write mb_write;
} __attribute__((packed));

//
// Prototypes

//
// Variables

//
// Functions
