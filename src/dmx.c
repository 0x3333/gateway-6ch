#include <unistd.h>

#include <FreeRTOS.h>
#include <semphr.h>

#include "defs.h"
#include "dmx.h"
#include "messages.h"
#include "utils.h"

#include "dmx.pio.h"
#include "macrologger.h"

struct dmx
{
    const uint tx_pin;
    const uint en_pin;

    PIO pio;
    uint sm;
    uint offset;
    uint dma_channel;
    dma_channel_config dma_config;
    uint8_t dma_buffer_length;
    uint8_t dma_buffer[DMX_MAX_CHANNELS + 1]; // 1 extra byte for start code
};

struct dmx dmx = {
    .tx_pin = BUS_6_TX_PIN,
    .en_pin = BUS_6_EN_PIN,
    .dma_buffer_length = 0,
    .dma_buffer = {0},
};

QueueHandle_t dmx_write_queue;

static void task_dmx_tx(void *arg);

void dmx_init()
{
    LOG_DEBUG("Initializing DMX");

    //
    // Initialize DMX PIO
    static int program_offset = PICO_ERROR_GENERIC;

    // Add program if not already
    if (program_offset <= PICO_ERROR_GENERIC)
    {
        program_offset = pio_add_program(DMX_PIO, &dmx_program);
    }
    dmx.offset = program_offset;
    dmx.pio = DMX_PIO;
    int sm = pio_claim_unused_sm(dmx.pio, true);
    if (sm == -1)
    {
        panic("No DMX State Machine available!");
    }
    dmx.sm = sm;
    dmx_program_init(dmx.pio, dmx.sm, (uint)program_offset, dmx.tx_pin, dmx.en_pin, DMX_BAUDRATE);

    // Initialize DMX DMA
    int dma_channel = dma_claim_unused_channel(true);
    if (dma_channel <= PICO_ERROR_GENERIC)
    {
        panic("No DMA Channel available for DMX!");
    }
    dmx.dma_channel = dma_channel;
    dmx.dma_config = dma_channel_get_default_config(dma_channel);
    channel_config_set_transfer_data_size(&dmx.dma_config, DMA_SIZE_8);
    channel_config_set_dreq(&dmx.dma_config, pio_get_dreq(dmx.pio, dmx.sm, true));
    channel_config_set_read_increment(&dmx.dma_config, true);
    channel_config_set_write_increment(&dmx.dma_config, false);
    dma_channel_set_write_addr(dmx.dma_channel, &dmx.pio->txf[dmx.sm], false);
    dma_channel_set_config(dmx.dma_channel, &dmx.dma_config, false);

    //
    // Initialize DMX Task

    dmx_write_queue = xQueueCreate(DMX_WRITE_QUEUE_LENGTH, sizeof(struct m_dmx_write));

    xTaskCreateAffinitySet(task_dmx_tx,
                           "DMX TX",
                           configMINIMAL_STACK_SIZE,
                           NULL,
                           tskLOW_PRIORITY,
                           HOST_TASK_CORE_AFFINITY,
                           NULL);
}

void dmx_write(const uint8_t *universe, uint8_t length)
{
    if (length > sizeof(dmx.dma_buffer))
    {
        LOG_ERROR("DMX universe length exceeds buffer size, output will be truncated!");
    }

    // Block task if DMA is still transmitting
    bool issued = false;
    while (!dmx_is_writable())
    {
        if (!issued)
        {
            issued = true;
            LOG_ERROR("DMX delaying write...");
        }
        taskYIELD();
    }

    // Transfer data array to DMA buffer
    dmx.dma_buffer[0] = 0; // Start code
    memcpy(dmx.dma_buffer + 1, universe, MIN(DMX_MAX_CHANNELS, length));

    // Restart SM and start DMA transfer
    pio_sm_set_enabled(dmx.pio, dmx.sm, false);
    pio_sm_restart(dmx.pio, dmx.sm);
    pio_sm_exec(dmx.pio, dmx.sm, pio_encode_jmp(dmx.offset));
    pio_sm_set_enabled(dmx.pio, dmx.sm, true);
    dma_channel_transfer_from_buffer_now(
        dmx.dma_channel,
        dmx.dma_buffer,
        MIN(sizeof(dmx.dma_buffer), length + 1));
}

bool dmx_is_writable()
{
    return !dma_channel_is_busy(dmx.dma_channel) && pio_sm_is_tx_fifo_empty(dmx.pio, dmx.sm);
}

_Noreturn static void task_dmx_tx(void *arg)
{
    (void)arg;
    struct m_dmx_write data = {.universe = {0}};

    TickType_t next_write = 0;

    struct m_dmx_write to_write = {.universe = {0}};

    for (;;) // Task infinite loop
    {
        if (xQueueReceive(dmx_write_queue, &to_write, 0))
        {
            memcpy(data.universe, to_write.universe, sizeof(to_write.universe));
            LOG_DEBUG("DMX Write %s", to_hex_string(data.universe, sizeof(to_write.universe)));
        }

        if (IS_EXPIRED(next_write))
        {
            dmx_write(data.universe, sizeof(data.universe));
            next_write = NEXT_TIMEOUT(DMX_DELAY_BETWEEN_WRITES);
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}