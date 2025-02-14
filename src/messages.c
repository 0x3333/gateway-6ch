#include "messages.h"

const struct m_handler m_handlers[] = {
    {
        .message_id = MESSAGE_CONFIG_BUS,
        .handler = (void (*)(const void *))handle_m_config_bus,
    },
    {
        .message_id = MESSAGE_READ,
        .handler = (void (*)(const void *))handle_m_read,
    },
    {
        .message_id = MESSAGE_WRITE,
        .handler = (void (*)(const void *))handle_m_write,
    },
    {
        .message_id = MESSAGE_PICO_RESET,
        .handler = (void (*)(const void *))handle_m_pico_reset,
    },
    {
        .message_id = MESSAGE_HEARTBEAT,
        .handler = (void (*)(const void *))handle_m_heartbeat,
    }};
