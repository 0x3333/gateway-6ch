#include <unistd.h>

#include "dmx.h"
#include "dmx.pio.h"

static ssize_t add_active_dmx(struct dmx *dmx)
{
    size_t i;
    for (i = 0; i < COUNT_PIO_DMX; i++)
    {
        if (active_dmxs[i] == NULL)
        {
            active_dmxs[i] = dmx;
            return i;
        }
    }
    return -1;
}

void dmx_init(struct dmx *const dmx)
{
    static int program_offset = PICO_ERROR_GENERIC;

    // Add this DMX to the active dmx array
    add_active_dmx(dmx);

    // Add program if not already
    if (program_offset <= PICO_ERROR_GENERIC)
    {
        program_offset = pio_add_program(DMX_PIO, &dmx_program);
    }
    dmx->pio = DMX_PIO;
    int sm = pio_claim_unused_sm(dmx->pio, true);
    if (sm == -1)
    {
        panic("No DMX State Machine available!");
    }
    dmx->sm = sm;
    dmx_program_init(dmx->pio, dmx->sm, (uint)program_offset, dmx->tx_pin, dmx->en_pin);

    // Initialize DMX DMA
    int dma = dma_claim_unused_channel(false);
    if (dma == -1)
        panic("No DMA channel available!");
    dmx->dma = dma;

    dma_channel_config dma_conf = dma_channel_get_default_config(dma);
    // Set the DMA to move one byte per DREQ signal
    channel_config_set_transfer_data_size(&dma_conf, DMA_SIZE_8);
    // Setup the DREQ so that the DMA only moves data when there
    // is available room in the TXF buffer of our PIO state machine
    channel_config_set_dreq(&dma_conf, pio_get_dreq(dmx->pio, dmx->sm, true));
    // Setup the DMA to write to the TXF buffer of the PIO state machine
    dma_channel_set_write_addr(dma, &dmx->pio->txf[dmx->sm], false);
    // Apply the config
    dma_channel_set_config(dma, &dma_conf, false);
}

void dmx_write(struct dmx *dmx, uint8_t *universe, size_t length)
{
    pio_sm_set_enabled(dmx->pio, dmx->sm, false);

    // Reset the PIO state machine to a consistent state. Clear the buffers and registers
    pio_sm_restart(dmx->pio, dmx->sm);

    // Start the DMX PIO program from the beginning
    pio_sm_exec(dmx->pio, dmx->sm, pio_encode_jmp(dmx->offset));

    // Restart the PIO state machinge
    pio_sm_set_enabled(dmx->pio, dmx->sm, true);

    // Start the DMA transfer
    dma_channel_transfer_from_buffer_now(dmx->dma, universe, length);
}

bool dmx_is_busy(struct dmx *dmx)
{
    if (dma_channel_is_busy(dmx->dma))
    {
        return true;
    }
    return !pio_sm_is_tx_fifo_empty(dmx->pio, dmx->sm);
}
