#include "ring_buffer.h"

// TODO: Criar uma versão de gets and puts no ring buffer.

void ring_buffer_init(ring_buffer_t *rb, uint8_t *buffer, uint8_t buffer_size)
{
    rb->buffer = buffer;
    rb->buffer_size = buffer_size;
    rb->head = 0;
    rb->tail = 0;
}

inline bool ring_buffer_put(ring_buffer_t *rb, uint8_t data)
{
    uint16_t next_head = (rb->head + 1) % rb->buffer_size;

    // Check if buffer is full
    if (next_head != rb->tail)
    {
        rb->buffer[rb->head] = data;
        rb->head = next_head;
        return true;
    }
    return false; // Buffer full
}

inline bool ring_buffer_get(ring_buffer_t *rb, uint8_t *data)
{
    if (rb->head != rb->tail)
    {
        *data = rb->buffer[rb->tail];
        rb->tail = (rb->tail + 1) % rb->buffer_size;
        return true;
    }
    return false; // Buffer empty
}

inline bool ring_buffer_is_empty(ring_buffer_t *rb)
{
    return (rb->head == rb->tail);
}

inline bool ring_buffer_is_full(ring_buffer_t *rb)
{
    return ((rb->head + 1) % rb->buffer_size == rb->tail);
}

inline uint8_t ring_buffer_used_space(ring_buffer_t *rb)
{
    if (rb->head >= rb->tail)
    {
        return rb->head - rb->tail;
    }
    return rb->buffer_size - (rb->tail - rb->head);
}