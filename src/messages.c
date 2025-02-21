#include "messages.h"

const struct m_handler m_handlers[] = {
    {
        .message_id = MESSAGE_CONFIG_BUS,
        .handler = (void (*)(const void *))handle_m_config_bus,
    },
    {
        .message_id = MESSAGE_COMMAND_READ,
        .handler = (void (*)(const void *))handle_m_command,
    },
    {
        .message_id = MESSAGE_COMMAND_WRITE,
        .handler = (void (*)(const void *))handle_m_command,
    },
    {
        .message_id = MESSAGE_DMX_WRITE,
        .handler = (void (*)(const void *))handle_m_dmx_write,
    },
    {
        .message_id = MESSAGE_PICO_RESET,
        .handler = (void (*)(const void *))handle_m_pico_reset,
    },
};
