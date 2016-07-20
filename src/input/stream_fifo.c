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
#include <vlc_stream.h>

#include "stream.h"

struct stream_sys_t
{
    vlc_fifo_t *fifo;
    bool eof;
};

static void vlc_stream_fifo_Destroy(stream_t *s)
{
    stream_sys_t *sys = s->p_sys;
    vlc_fifo_t *fifo = sys->fifo;
    block_t *block;
    bool closed;

    vlc_fifo_Lock(fifo);
    block = vlc_fifo_DequeueAllUnlocked(fifo);
    closed = sys->eof;
    sys->eof = true;
    vlc_fifo_Unlock(fifo);

    block_ChainRelease(block);

    if (closed)
    {   /* Destroy shared state if write end is already closed */
        block_FifoRelease(fifo);
        free(sys);
    }
}

static block_t *vlc_stream_fifo_Block(stream_t *s, bool *restrict eof)
{
    stream_sys_t *sys = s->p_sys;
    vlc_fifo_t *fifo = sys->fifo;
    block_t *block;

    vlc_fifo_Lock(fifo);
    while (vlc_fifo_IsEmpty(fifo))
    {
        if (sys->eof)
        {
            *eof = true;
            break;
        }
        vlc_fifo_Wait(fifo);
    }

    block = vlc_fifo_DequeueUnlocked(fifo);
    vlc_fifo_Unlock(fifo);
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
            *va_arg(ap, int64_t *) = DEFAULT_PTS_DELAY;
            break;

        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

stream_t *vlc_stream_fifo_New(vlc_object_t *parent)
{
    stream_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return NULL;

    sys->fifo = block_FifoNew();
    if (unlikely(sys->fifo == NULL))
    {
        free(sys);
        return NULL;
    }

    sys->eof = false;

    stream_t *s = vlc_stream_CommonNew(parent, vlc_stream_fifo_Destroy);
    if (unlikely(s == NULL))
    {
        block_FifoRelease(sys->fifo);
        free(sys);
        return NULL;
    }

    s->pf_block = vlc_stream_fifo_Block;
    s->pf_seek = NULL;
    s->pf_control = vlc_stream_fifo_Control;
    s->p_sys = sys;
    return vlc_object_hold(s);
}

int vlc_stream_fifo_Queue(stream_t *s, block_t *block)
{
    stream_sys_t *sys = s->p_sys;
    vlc_fifo_t *fifo = sys->fifo;

    vlc_fifo_Lock(fifo);
    if (likely(!sys->eof))
    {
        vlc_fifo_QueueUnlocked(fifo, block);
        block = NULL;
    }
    vlc_fifo_Unlock(fifo);

    if (unlikely(block != NULL))
    {
        block_Release(block);
        errno = EPIPE;
        return -1;
    }
    return 0;
}

ssize_t vlc_stream_fifo_Write(stream_t *s, const void *buf, size_t len)
{
    block_t *block = block_Alloc(len);
    if (unlikely(block == NULL))
        return -1;

    memcpy(block->p_buffer, buf, len);
    return vlc_stream_fifo_Queue(s, block) ? -1 : (ssize_t)len;
}

void vlc_stream_fifo_Close(stream_t *s)
{
    stream_sys_t *sys = s->p_sys;
    vlc_fifo_t *fifo = sys->fifo;
    bool closed;

    vlc_fifo_Lock(fifo);
    closed = sys->eof;
    sys->eof = true;
    vlc_fifo_Signal(fifo);
    vlc_fifo_Unlock(fifo);

    if (closed)
    {   /* Destroy shared state if read end is already closed */
        block_FifoRelease(fifo);
        free(sys);
    }

    vlc_object_release(s);
}
