#include "messages.h"

const struct m_handler m_handlers[] = {
    {
        .message_id = MESSAGE_CONFIG_BUS_ID,
        .handler = (void (*)(const void *))handle_m_config_bus,
    },
    {
        .message_id = MESSAGE_READ_ID,
        .handler = (void (*)(const void *))handle_m_read,
    },
    {
        .message_id = MESSAGE_WRITE_ID,
        .handler = (void (*)(const void *))handle_m_write,
    }};
