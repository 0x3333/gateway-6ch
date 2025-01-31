#ifndef RING_BUFFER_H_
#define RING_BUFFER_H_

#include <stdint.h>
#include <stdbool.h>

// Note: This implementation is limited to 255 elements(uint8_t)

typedef struct
{
    uint8_t *buffer;
    uint8_t buffer_size;
    uint8_t head;
    uint8_t tail;
} ring_buffer_t;

// Initialize the circular buffer
void ring_buffer_init(ring_buffer_t *rb, uint8_t *buffer, uint8_t buffer_size);

bool ring_buffer_put(ring_buffer_t *rb, uint8_t data);

bool ring_buffer_get(ring_buffer_t *rb, uint8_t *data);

bool ring_buffer_is_empty(ring_buffer_t *rb);

bool ring_buffer_is_full(ring_buffer_t *rb);

uint8_t ring_buffer_used_space(ring_buffer_t *rb);

void ring_buffer_clear(ring_buffer_t *rb);

#endif // RING_BUFFER_H_