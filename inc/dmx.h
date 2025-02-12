#ifndef DMX_H_
#define DMX_H_

#include "hardware/pio.h"

//
// Defines
//

#define COUNT_PIO_DMX 1u

#ifndef DMX_PIO
#define DMX_PIO pio1
#endif

//
// Data Structures
//

struct dmx
{
    const uint tx_pin;
    const uint en_pin;

    PIO pio;
    uint sm;
    uint dma;
    uint offset;
};

//
// Variables
//

// Null terminated array of PIO DMXs
extern struct dmx *active_dmxs[COUNT_PIO_DMX + 1];

//
// Prototypes
//

void dmx_init(struct dmx *const dmx);

void dmx_write(struct dmx *const dmx, uint8_t *universe, size_t length);

bool dmx_is_busy(struct dmx *const dmx);

#endif