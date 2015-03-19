/*****************************************************************************
 * fifo.c: FIFO management functions
 *****************************************************************************
 * Copyright (C) 2003-2004 VLC authors and VideoLAN
 * Copyright (C) 2007-2009 Rémi Denis-Courmont
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

/**
 * @section Thread-safe block queue functions
 */

/**
 * Internal state for block queues
 */
struct block_fifo_t
{
    vlc_mutex_t         lock;                         /* fifo data lock */
    vlc_cond_t          wait;      /**< Wait for data */
    vlc_cond_t          wait_room; /**< Wait for queue depth to shrink */

    block_t             *p_first;
    block_t             **pp_last;
    size_t              i_depth;
    size_t              i_size;
    bool          b_force_wake;
};

/**
 * Creates a thread-safe FIFO queue of blocks.
 * See also block_FifoPut() and block_FifoGet().
 * @return the FIFO or NULL on memory error
 */
block_fifo_t *block_FifoNew( void )
{
    block_fifo_t *p_fifo = malloc( sizeof( block_fifo_t ) );
    if( !p_fifo )
        return NULL;

    vlc_mutex_init( &p_fifo->lock );
    vlc_cond_init( &p_fifo->wait );
    vlc_cond_init( &p_fifo->wait_room );
    p_fifo->p_first = NULL;
    p_fifo->pp_last = &p_fifo->p_first;
    p_fifo->i_depth = p_fifo->i_size = 0;
    p_fifo->b_force_wake = false;

    return p_fifo;
}

/**
 * Destroys a FIFO created by block_FifoNew().
 * Any queued blocks are also destroyed.
 */
void block_FifoRelease( block_fifo_t *p_fifo )
{
    block_ChainRelease( p_fifo->p_first );
    vlc_cond_destroy( &p_fifo->wait_room );
    vlc_cond_destroy( &p_fifo->wait );
    vlc_mutex_destroy( &p_fifo->lock );
    free( p_fifo );
}

void block_FifoEmpty( block_fifo_t *p_fifo )
{
    block_t *block;

    vlc_mutex_lock( &p_fifo->lock );
    block = p_fifo->p_first;
    if (block != NULL)
    {
        p_fifo->i_depth = p_fifo->i_size = 0;
        p_fifo->p_first = NULL;
        p_fifo->pp_last = &p_fifo->p_first;
    }
    vlc_cond_broadcast( &p_fifo->wait_room );
    vlc_mutex_unlock( &p_fifo->lock );

    while (block != NULL)
    {
        block_t *buf;

        buf = block->p_next;
        block_Release (block);
        block = buf;
    }
}

/**
 * Wait until the FIFO gets below a certain size (if needed).
 *
 * Note that if more than one thread writes to the FIFO, you cannot assume that
 * the FIFO is actually below the requested size upon return (since another
 * thread could have refilled it already). This is typically not an issue, as
 * this function is meant for (relaxed) congestion control.
 *
 * This function may be a cancellation point and it is cancel-safe.
 *
 * @param fifo queue to wait on
 * @param max_depth wait until the queue has no more than this many blocks
 *                  (use SIZE_MAX to ignore this constraint)
 * @param max_size wait until the queue has no more than this many bytes
 *                  (use SIZE_MAX to ignore this constraint)
 * @return nothing.
 */
void block_FifoPace (block_fifo_t *fifo, size_t max_depth, size_t max_size)
{
    vlc_testcancel ();

    vlc_mutex_lock (&fifo->lock);
    while ((fifo->i_depth > max_depth) || (fifo->i_size > max_size))
    {
         mutex_cleanup_push (&fifo->lock);
         vlc_cond_wait (&fifo->wait_room, &fifo->lock);
         vlc_cleanup_pop ();
    }
    vlc_mutex_unlock (&fifo->lock);
}

/**
 * Immediately queue one block at the end of a FIFO.
 * @param fifo queue
 * @param block head of a block list to queue (may be NULL)
 * @return total number of bytes appended to the queue
 */
size_t block_FifoPut( block_fifo_t *p_fifo, block_t *p_block )
{
    size_t i_size = 0, i_depth = 0;
    block_t *p_last;

    if (p_block == NULL)
        return 0;
    for (p_last = p_block; ; p_last = p_last->p_next)
    {
        i_size += p_last->i_buffer;
        i_depth++;
        if (!p_last->p_next)
            break;
    }

    vlc_mutex_lock (&p_fifo->lock);
    *p_fifo->pp_last = p_block;
    p_fifo->pp_last = &p_last->p_next;
    p_fifo->i_depth += i_depth;
    p_fifo->i_size += i_size;
    /* We queued at least one block: wake up one read-waiting thread */
    vlc_cond_signal( &p_fifo->wait );
    vlc_mutex_unlock( &p_fifo->lock );

    return i_size;
}

void block_FifoWake( block_fifo_t *p_fifo )
{
    vlc_mutex_lock( &p_fifo->lock );
    if( p_fifo->p_first == NULL )
        p_fifo->b_force_wake = true;
    vlc_cond_broadcast( &p_fifo->wait );
    vlc_mutex_unlock( &p_fifo->lock );
}

/**
 * Dequeue the first block from the FIFO. If necessary, wait until there is
 * one block in the queue. This function is (always) cancellation point.
 *
 * @return a valid block, or NULL if block_FifoWake() was called.
 */
block_t *block_FifoGet( block_fifo_t *p_fifo )
{
    block_t *b;

    vlc_testcancel( );

    vlc_mutex_lock( &p_fifo->lock );
    mutex_cleanup_push( &p_fifo->lock );

    /* Remember vlc_cond_wait() may cause spurious wakeups
     * (on both Win32 and POSIX) */
    while( ( p_fifo->p_first == NULL ) && !p_fifo->b_force_wake )
        vlc_cond_wait( &p_fifo->wait, &p_fifo->lock );

    vlc_cleanup_pop();
    b = p_fifo->p_first;

    p_fifo->b_force_wake = false;
    if( b == NULL )
    {
        /* Forced wakeup */
        vlc_mutex_unlock( &p_fifo->lock );
        return NULL;
    }

    p_fifo->p_first = b->p_next;
    p_fifo->i_depth--;
    p_fifo->i_size -= b->i_buffer;

    if( p_fifo->p_first == NULL )
    {
        p_fifo->pp_last = &p_fifo->p_first;
    }

    /* We don't know how many threads can queue new packets now. */
    vlc_cond_broadcast( &p_fifo->wait_room );
    vlc_mutex_unlock( &p_fifo->lock );

    b->p_next = NULL;
    return b;
}

/**
 * Peeks the first block in the FIFO.
 * If necessary, wait until there is one block.
 * This function is (always) a cancellation point.
 *
 * @warning This function leaves the block in the FIFO.
 * You need to protect against concurrent threads who could dequeue the block.
 * Preferrably, there should be only one thread reading from the FIFO.
 *
 * @return a valid block.
 */
block_t *block_FifoShow( block_fifo_t *p_fifo )
{
    block_t *b;

    vlc_testcancel( );

    vlc_mutex_lock( &p_fifo->lock );
    mutex_cleanup_push( &p_fifo->lock );

    while( p_fifo->p_first == NULL )
        vlc_cond_wait( &p_fifo->wait, &p_fifo->lock );

    b = p_fifo->p_first;

    vlc_cleanup_run ();
    return b;
}

/* FIXME: not (really) thread-safe */
size_t block_FifoSize (block_fifo_t *fifo)
{
    size_t size;

    vlc_mutex_lock (&fifo->lock);
    size = fifo->i_size;
    vlc_mutex_unlock (&fifo->lock);
    return size;
}

/* FIXME: not (really) thread-safe */
size_t block_FifoCount (block_fifo_t *fifo)
{
    size_t depth;

    vlc_mutex_lock (&fifo->lock);
    depth = fifo->i_depth;
    vlc_mutex_unlock (&fifo->lock);
    return depth;
}
