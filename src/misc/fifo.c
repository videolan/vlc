/*****************************************************************************
 * fifo.c: FIFO management functions
 *****************************************************************************
 * Copyright (C) 2003-2004 VLC authors and VideoLAN
 * Copyright (C) 2007-2015 Rémi Denis-Courmont
 *
 * Authors: Laurent Aimar <fenrir@videolan.org>
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
#include <stdlib.h>

#include <vlc_common.h>
#include <vlc_block.h>
#include "libvlc.h"

/**
 * Internal state for block queues
 */
struct block_fifo_t
{
    vlc_queue_t         q;
    size_t              i_depth;
    size_t              i_size;
};

static_assert (offsetof (block_fifo_t, q) == 0, "Problems in <vlc_block.h>");

size_t vlc_fifo_GetCount(const vlc_fifo_t *fifo)
{
    vlc_mutex_assert(&fifo->q.lock);
    return fifo->i_depth;
}

size_t vlc_fifo_GetBytes(const vlc_fifo_t *fifo)
{
    vlc_mutex_assert(&fifo->q.lock);
    return fifo->i_size;
}

void vlc_fifo_QueueUnlocked(block_fifo_t *fifo, block_t *block)
{
    for (block_t *b = block; b != NULL; b = b->p_next) {
        fifo->i_depth++;
        fifo->i_size += b->i_buffer;
    }

    vlc_queue_EnqueueUnlocked(&fifo->q, block);
}

block_t *vlc_fifo_DequeueUnlocked(block_fifo_t *fifo)
{
    block_t *block = vlc_queue_DequeueUnlocked(&fifo->q);

    if (block != NULL) {
        assert(fifo->i_depth > 0);
        assert(fifo->i_size >= block->i_buffer);
        fifo->i_depth--;
        fifo->i_size -= block->i_buffer;
    }

    return block;
}

block_t *vlc_fifo_DequeueAllUnlocked(block_fifo_t *fifo)
{
    fifo->i_depth = 0;
    fifo->i_size = 0;
    return vlc_queue_DequeueAllUnlocked(&fifo->q);
}

block_fifo_t *block_FifoNew( void )
{
    block_fifo_t *p_fifo = malloc( sizeof( block_fifo_t ) );

    if (likely(p_fifo != NULL)) {
        vlc_queue_Init(&p_fifo->q, offsetof (block_t, p_next));
        p_fifo->i_depth = 0;
        p_fifo->i_size = 0;
    }

    return p_fifo;
}

void block_FifoRelease( block_fifo_t *p_fifo )
{
    block_FifoEmpty(p_fifo);
    free( p_fifo );
}

block_t *block_FifoGet(block_fifo_t *fifo)
{
    block_t *block;

    vlc_testcancel();

    vlc_fifo_Lock(fifo);
    while (vlc_fifo_IsEmpty(fifo))
    {
        vlc_fifo_CleanupPush(fifo);
        vlc_fifo_Wait(fifo);
        vlc_cleanup_pop();
    }
    block = vlc_fifo_DequeueUnlocked(fifo);
    vlc_fifo_Unlock(fifo);

    return block;
}

block_t *block_FifoShow( block_fifo_t *p_fifo )
{
    block_t *b;

    vlc_fifo_Lock(p_fifo);
    assert(p_fifo->q.first != NULL);
    b = (block_t *)p_fifo->q.first;
    vlc_fifo_Unlock(p_fifo);

    return b;
}
