/*****************************************************************************
 * block.c: Data blocks management functions
 *****************************************************************************
 * Copyright (C) 2003-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>
#include "vlc_block.h"

/*****************************************************************************
 * Block functions.
 *****************************************************************************/
/* private */
struct block_sys_t
{
    block_t     self;
    size_t      i_allocated_buffer;
    uint8_t     p_allocated_buffer[0];
};

#ifndef NDEBUG
static void BlockNoRelease( block_t *b )
{
    fprintf( stderr, "block %p has no release callback! This is a bug!\n", b );
    abort();
}
#endif

void block_Init( block_t *restrict b, void *buf, size_t size )
{
    /* Fill all fields to their default */
    b->p_next = b->p_prev = NULL;
    b->i_flags = 0;
    b->i_pts = b->i_dts = b->i_length = 0;
    b->i_rate = 0;
    b->p_buffer = buf;
    b->i_buffer = size;
#ifndef NDEBUG
    b->pf_release = BlockNoRelease;
#endif
}

static void BlockRelease( block_t *p_block )
{
    free( p_block );
}

#define BLOCK_PADDING_SIZE 32

block_t *block_Alloc( size_t i_size )
{
    /* We do only one malloc
     * TODO bench if doing 2 malloc but keeping a pool of buffer is better
     * 16 -> align on 16
     * 2 * BLOCK_PADDING_SIZE -> pre + post padding
     */
    const size_t i_alloc = i_size + 2 * BLOCK_PADDING_SIZE + 16;
    block_sys_t *p_sys = malloc( sizeof( *p_sys ) + i_alloc );

    if( p_sys == NULL )
        return NULL;

    /* Fill opaque data */
    p_sys->i_allocated_buffer = i_alloc;

    block_Init( &p_sys->self, p_sys->p_allocated_buffer + BLOCK_PADDING_SIZE
                + 16 - ((uintptr_t)p_sys->p_allocated_buffer % 16 ), i_size );
    p_sys->self.pf_release    = BlockRelease;

    return &p_sys->self;
}

block_t *block_Realloc( block_t *p_block, ssize_t i_prebody, size_t i_body )
{
    block_sys_t *p_sys = (block_sys_t *)p_block;
    ssize_t i_buffer_size;

    if( p_block->pf_release != BlockRelease )
    {
        /* Special case when pf_release if overloaded
         * TODO if used one day, them implement it in a smarter way */
        block_t *p_dup = block_Duplicate( p_block );
        block_Release( p_block );
        if( !p_dup )
            return NULL;

        p_block = p_dup;
    }

    i_buffer_size = i_prebody + i_body;

    if( i_buffer_size <= 0 )
    {
        block_Release( p_block );
        return NULL;
    }

    if( p_block->p_buffer - i_prebody > p_sys->p_allocated_buffer &&
        p_block->p_buffer - i_prebody < p_sys->p_allocated_buffer +
        p_sys->i_allocated_buffer )
    {
        p_block->p_buffer -= i_prebody;
        p_block->i_buffer += i_prebody;
        i_prebody = 0;
    }
    if( p_block->p_buffer + i_body < p_sys->p_allocated_buffer +
        p_sys->i_allocated_buffer )
    {
        p_block->i_buffer = i_buffer_size;
        i_body = 0;
    }

    if( i_body > 0 || i_prebody > 0 )
    {
        block_t *p_rea = block_New( NULL, i_buffer_size );

        if( p_rea )
        {
            p_rea->i_dts     = p_block->i_dts;
            p_rea->i_pts     = p_block->i_pts;
            p_rea->i_flags   = p_block->i_flags;
            p_rea->i_length  = p_block->i_length;
            p_rea->i_rate    = p_block->i_rate;
            p_rea->i_samples = p_block->i_samples;

            memcpy( p_rea->p_buffer + i_prebody, p_block->p_buffer,
                    __MIN( p_block->i_buffer, p_rea->i_buffer - i_prebody ) );
        }

        block_Release( p_block );

        return p_rea;
    }

    return p_block;
}

#ifdef HAVE_MMAP
# include <sys/mman.h>

typedef struct block_mmap_t
{
    block_t     self;
    void       *base_addr;
    size_t      length;
} block_mmap_t;

static void block_mmap_Release (block_t *block)
{
    block_mmap_t *p_sys = (block_mmap_t *)block;

    munmap (p_sys->base_addr, p_sys->length);
    free (p_sys);
}

block_t *block_mmap_Alloc (void *addr, size_t length)
{
    if (addr == MAP_FAILED)
        return NULL;

    block_mmap_t *block = malloc (sizeof (*block));
    if (block == NULL)
    {
        munmap (addr, length);
        return NULL;
    }

    block_Init (&block->self, (uint8_t *)addr, length);
    block->self.pf_release = block_mmap_Release;
    block->base_addr = addr;
    block->length = length;
    return &block->self;
}
#endif

/*****************************************************************************
 * block_fifo_t management
 *****************************************************************************/
struct block_fifo_t
{
    vlc_mutex_t         lock;                         /* fifo data lock */
    vlc_cond_t          wait;         /* fifo data conditional variable */

    block_t             *p_first;
    block_t             **pp_last;
    size_t              i_depth;
    size_t              i_size;
    vlc_bool_t          b_force_wake;
};

block_fifo_t *__block_FifoNew( vlc_object_t *p_obj )
{
    (void)p_obj;

    block_fifo_t *p_fifo;

    p_fifo = malloc( sizeof( block_fifo_t ) );
    if( !p_fifo ) return NULL;
    vlc_mutex_init( p_obj, &p_fifo->lock );
    vlc_cond_init( p_obj, &p_fifo->wait );
    p_fifo->p_first = NULL;
    p_fifo->pp_last = &p_fifo->p_first;
    p_fifo->i_depth = p_fifo->i_size = 0;
    p_fifo->b_force_wake = VLC_FALSE;

    return p_fifo;
}

void block_FifoRelease( block_fifo_t *p_fifo )
{
    block_FifoEmpty( p_fifo );
    vlc_cond_destroy( &p_fifo->wait );
    vlc_mutex_destroy( &p_fifo->lock );
    free( p_fifo );
}

void block_FifoEmpty( block_fifo_t *p_fifo )
{
    block_t *b;

    vlc_mutex_lock( &p_fifo->lock );
    for( b = p_fifo->p_first; b != NULL; )
    {
        block_t *p_next;

        p_next = b->p_next;
        block_Release( b );
        b = p_next;
    }

    p_fifo->i_depth = p_fifo->i_size = 0;
    p_fifo->p_first = NULL;
    p_fifo->pp_last = &p_fifo->p_first;
    vlc_mutex_unlock( &p_fifo->lock );
}

size_t block_FifoPut( block_fifo_t *p_fifo, block_t *p_block )
{
    size_t i_size = 0;
    vlc_mutex_lock( &p_fifo->lock );

    do
    {
        i_size += p_block->i_buffer;

        *p_fifo->pp_last = p_block;
        p_fifo->pp_last = &p_block->p_next;
        p_fifo->i_depth++;
        p_fifo->i_size += p_block->i_buffer;

        p_block = p_block->p_next;

    } while( p_block );

    /* warn there is data in this fifo */
    vlc_cond_signal( &p_fifo->wait );
    vlc_mutex_unlock( &p_fifo->lock );

    return i_size;
}

void block_FifoWake( block_fifo_t *p_fifo )
{
    vlc_mutex_lock( &p_fifo->lock );
    if( p_fifo->p_first == NULL )
        p_fifo->b_force_wake = VLC_TRUE;
    vlc_cond_signal( &p_fifo->wait );
    vlc_mutex_unlock( &p_fifo->lock );
}

block_t *block_FifoGet( block_fifo_t *p_fifo )
{
    block_t *b;

    vlc_mutex_lock( &p_fifo->lock );

    /* Remember vlc_cond_wait() may cause spurious wakeups
     * (on both Win32 and POSIX) */
    while( ( p_fifo->p_first == NULL ) && !p_fifo->b_force_wake )
    {
        vlc_cond_wait( &p_fifo->wait, &p_fifo->lock );
    }

    b = p_fifo->p_first;

    p_fifo->b_force_wake = VLC_FALSE;
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

    vlc_mutex_unlock( &p_fifo->lock );

    b->p_next = NULL;
    return b;
}

block_t *block_FifoShow( block_fifo_t *p_fifo )
{
    block_t *b;

    vlc_mutex_lock( &p_fifo->lock );

    if( p_fifo->p_first == NULL )
    {
        vlc_cond_wait( &p_fifo->wait, &p_fifo->lock );
    }

    b = p_fifo->p_first;

    vlc_mutex_unlock( &p_fifo->lock );

    return( b );
}

size_t block_FifoSize( const block_fifo_t *p_fifo )
{
    return p_fifo->i_size;
}

size_t block_FifoCount( const block_fifo_t *p_fifo )
{
    return p_fifo->i_depth;
}
