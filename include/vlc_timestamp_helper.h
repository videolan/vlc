/*****************************************************************************
 * vlc_timestamp_helper.h : timestamp handling helpers
 *****************************************************************************
 * Copyright (C) 2014 VLC authors and VideoLAN
 *
 * Authors: Felix Abecassis <felix.abecassis@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_TIMESTAMP_H
#define VLC_TIMESTAMP_H 1

/* Implementation of a circular buffer of timestamps with overwriting
 * of older values. MediaCodec has only one type of timestamp, if a
 * block has no PTS, we send the DTS instead. Some hardware decoders
 * cannot cope with this situation and output the frames in the wrong
 * order. As a workaround in this case, we use a FIFO of timestamps in
 * order to remember which input packets had no PTS.  Since an
 * hardware decoder can silently drop frames, this might cause a
 * growing desynchronization with the actual timestamp. Thus the
 * circular buffer has a limited size and will overwrite older values.
 */
typedef struct
{
    uint32_t          begin;
    uint32_t          size;
    uint32_t          capacity;
    vlc_tick_t        *buffer;
} timestamp_fifo_t;

static inline timestamp_fifo_t *timestamp_FifoNew(uint32_t capacity)
{
    timestamp_fifo_t *fifo = (timestamp_fifo_t *)calloc(1, sizeof(*fifo));
    if (!fifo)
        return NULL;
    fifo->buffer = (vlc_tick_t*)vlc_alloc(capacity, sizeof(*fifo->buffer));
    if (!fifo->buffer) {
        free(fifo);
        return NULL;
    }
    fifo->capacity = capacity;
    return fifo;
}

static inline void timestamp_FifoRelease(timestamp_fifo_t *fifo)
{
    free(fifo->buffer);
    free(fifo);
}

static inline bool timestamp_FifoIsEmpty(timestamp_fifo_t *fifo)
{
    return fifo->size == 0;
}

static inline bool timestamp_FifoIsFull(timestamp_fifo_t *fifo)
{
    return fifo->size == fifo->capacity;
}

static inline void timestamp_FifoEmpty(timestamp_fifo_t *fifo)
{
    fifo->size = 0;
}

static inline void timestamp_FifoPut(timestamp_fifo_t *fifo, vlc_tick_t ts)
{
    uint32_t end = (fifo->begin + fifo->size) % fifo->capacity;
    fifo->buffer[end] = ts;
    if (!timestamp_FifoIsFull(fifo))
        fifo->size += 1;
    else
        fifo->begin = (fifo->begin + 1) % fifo->capacity;
}

static inline vlc_tick_t timestamp_FifoGet(timestamp_fifo_t *fifo)
{
    if (timestamp_FifoIsEmpty(fifo))
        return VLC_TICK_INVALID;

    vlc_tick_t result = fifo->buffer[fifo->begin];
    fifo->begin = (fifo->begin + 1) % fifo->capacity;
    fifo->size -= 1;
    return result;
}

#endif
