/*****************************************************************************
 * stream_fifo.c
 *****************************************************************************
 * Copyright (C) 2016 Rémi Denis-Courmont
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>

#include <vlc_common.h>
#include <vlc_block.h>
#include <vlc_queue.h>
#include <vlc_stream.h>

#include "stream.h"

struct vlc_stream_fifo {
    vlc_queue_t queue;
    bool eof;
};

struct vlc_stream_fifo_private {
    vlc_stream_fifo_t *writer;
};

static vlc_stream_fifo_t *vlc_stream_fifo_Writer(stream_t *s)
{
    struct vlc_stream_fifo_private *sys = vlc_stream_Private(s);

    return sys->writer;
}

static void vlc_stream_fifo_Destroy(stream_t *s)
{
    struct vlc_stream_fifo *writer = vlc_stream_fifo_Writer(s);
    block_t *block;
    bool closed;

    vlc_queue_Lock(&writer->queue);
    block = vlc_queue_DequeueAllUnlocked(&writer->queue);
    closed = writer->eof;
    writer->eof = true;
    vlc_queue_Unlock(&writer->queue);

    block_ChainRelease(block);

    if (closed)
        /* Destroy shared state if write end is already closed */
        free(writer);
}

static block_t *vlc_stream_fifo_Block(stream_t *s, bool *restrict eof)
{
    struct vlc_stream_fifo *sys = vlc_stream_fifo_Writer(s);
    block_t *block = vlc_queue_DequeueKillable(&sys->queue, &sys->eof);

    if (block == NULL)
        *eof = true;

    return block;
}

static int vlc_stream_fifo_Control(stream_t *s, int query, va_list ap)
{
    (void) s;

    switch (query)
    {
        case STREAM_CAN_SEEK:
        case STREAM_CAN_FASTSEEK:
        case STREAM_CAN_PAUSE:
        case STREAM_CAN_CONTROL_PACE:
            *va_arg(ap, bool *) = false;
            break;

        case STREAM_GET_PTS_DELAY:
            *va_arg(ap, vlc_tick_t *) = DEFAULT_PTS_DELAY;
            break;

        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

vlc_stream_fifo_t *vlc_stream_fifo_New(vlc_object_t *parent, stream_t **reader)
{
    struct vlc_stream_fifo *writer = malloc(sizeof (*writer));
    if (unlikely(writer == NULL))
        return NULL;

    vlc_queue_Init(&writer->queue, offsetof (block_t, p_next));
    writer->eof = false;

    struct vlc_stream_fifo_private *sys;
    stream_t *s = vlc_stream_CustomNew(parent, vlc_stream_fifo_Destroy,
                                       sizeof (*sys), "stream");
    if (unlikely(s == NULL)) {
        free(writer);
        return NULL;
    }

    sys = vlc_stream_Private(s);
    sys->writer = writer;
    s->pf_block = vlc_stream_fifo_Block;
    s->pf_seek = NULL;
    s->pf_control = vlc_stream_fifo_Control;
    *reader = s;
    return writer;
}

int vlc_stream_fifo_Queue(vlc_stream_fifo_t *writer, block_t *block)
{
    vlc_queue_Lock(&writer->queue);
    if (likely(!writer->eof))
    {
        vlc_queue_EnqueueUnlocked(&writer->queue, block);
        block = NULL;
    }
    vlc_queue_Unlock(&writer->queue);

    if (unlikely(block != NULL))
    {
        block_Release(block);
        errno = EPIPE;
        return -1;
    }
    return 0;
}

ssize_t vlc_stream_fifo_Write(vlc_stream_fifo_t *writer,
                              const void *buf, size_t len)
{
    block_t *block = block_Alloc(len);
    if (unlikely(block == NULL))
        return -1;

    memcpy(block->p_buffer, buf, len);
    return vlc_stream_fifo_Queue(writer, block) ? -1 : (ssize_t)len;
}

void vlc_stream_fifo_Close(vlc_stream_fifo_t *writer)
{
    bool closed;

    vlc_queue_Lock(&writer->queue);
    closed = writer->eof;
    writer->eof = true;
    vlc_queue_Signal(&writer->queue);
    vlc_queue_Unlock(&writer->queue);

    if (closed)
        /* Destroy shared state if read end is already closed */
        free(writer);
}
