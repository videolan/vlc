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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>

#include <vlc/vlc.h>
#include "vlc_block.h"

/*****************************************************************************
 * Block functions.
 *****************************************************************************/
/* private */
struct block_sys_t
{
    uint8_t     *p_allocated_buffer;
    int         i_allocated_buffer;
};

#define BLOCK_PADDING_SIZE 32
static void BlockRelease( block_t * );

block_t *__block_New( vlc_object_t *p_obj, int i_size )
{
    /* We do only one malloc
     * TODO bench if doing 2 malloc but keeping a pool of buffer is better
     * 16 -> align on 16
     * 2 * BLOCK_PADDING_SIZE -> pre + post padding
     */
    block_sys_t *p_sys;
    const int i_alloc = i_size + 2 * BLOCK_PADDING_SIZE + 16;
    block_t *p_block =
        malloc( sizeof( block_t ) + sizeof( block_sys_t ) + i_alloc );

    if( p_block == NULL ) return NULL;

    /* Fill opaque data */
    p_sys = (block_sys_t*)( (uint8_t*)p_block + sizeof( block_t ) );
    p_sys->i_allocated_buffer = i_alloc;
    p_sys->p_allocated_buffer = (uint8_t*)p_block + sizeof( block_t ) +
        sizeof( block_sys_t );

    /* Fill all fields */
    p_block->p_next         = NULL;
    p_block->p_prev         = NULL;
    p_block->i_flags        = 0;
    p_block->i_pts          = 0;
    p_block->i_dts          = 0;
    p_block->i_length       = 0;
    p_block->i_rate         = 0;
    p_block->i_seqno        = 0;
    p_block->i_buffer       = i_size;
    p_block->p_buffer       =
        &p_sys->p_allocated_buffer[BLOCK_PADDING_SIZE +
            16 - ((uintptr_t)p_sys->p_allocated_buffer % 16 )];
    p_block->pf_release     = BlockRelease;

    /* Is ok, as no comunication between p_vlc */
    p_block->p_manager      = VLC_OBJECT( p_obj->p_vlc );
    p_block->p_sys          = p_sys;

    return p_block;
}

block_t *block_Realloc( block_t *p_block, int i_prebody, int i_body )
{
    int i_buffer_size;

    if( p_block->pf_release != BlockRelease )
    {
        /* Special case when pf_release if overloaded
         * TODO if used one day, them implement it in a smarter way */
        block_t *p_dup = block_Duplicate( p_block );
        block_Release( p_block );

        p_block = p_dup;
    }

    i_buffer_size = i_prebody + i_body;

    if( i_body < 0 || i_buffer_size <= 0 ) return NULL;

    if( p_block->p_buffer - i_prebody > p_block->p_sys->p_allocated_buffer &&
        p_block->p_buffer - i_prebody < p_block->p_sys->p_allocated_buffer +
        p_block->p_sys->i_allocated_buffer )
    {
        p_block->p_buffer -= i_prebody;
        p_block->i_buffer += i_prebody;
        i_prebody = 0;
    }
    if( p_block->p_buffer + i_body < p_block->p_sys->p_allocated_buffer +
        p_block->p_sys->i_allocated_buffer )
    {
        p_block->i_buffer = i_buffer_size;
        i_body = 0;
    }

    if( i_body > 0 || i_prebody > 0 )
    {
        block_t *p_rea = block_New( p_block->p_manager, i_buffer_size );

        p_rea->i_dts     = p_block->i_dts;
        p_rea->i_pts     = p_block->i_pts;
        p_rea->i_flags   = p_block->i_flags;
        p_rea->i_length  = p_block->i_length;
        p_rea->i_rate    = p_block->i_rate;
        p_rea->i_samples = p_block->i_samples;

        memcpy( p_rea->p_buffer + i_prebody, p_block->p_buffer,
                __MIN( p_block->i_buffer, p_rea->i_buffer - i_prebody ) );

        block_Release( p_block );

        return p_rea;
    }

    return p_block;
}

static void BlockRelease( block_t *p_block )
{
    free( p_block );
}


/*****************************************************************************
 * block_fifo_t management
 *****************************************************************************/
block_fifo_t *__block_FifoNew( vlc_object_t *p_obj )
{
    block_fifo_t *p_fifo;

    p_fifo = malloc( sizeof( vlc_object_t ) );
    vlc_mutex_init( p_obj, &p_fifo->lock );
    vlc_cond_init( p_obj, &p_fifo->wait );
    p_fifo->i_depth = p_fifo->i_size = 0;
    p_fifo->p_first = NULL;
    p_fifo->pp_last = &p_fifo->p_first;

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

int block_FifoPut( block_fifo_t *p_fifo, block_t *p_block )
{
    int i_size = 0;
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

block_t *block_FifoGet( block_fifo_t *p_fifo )
{
    block_t *b;

    vlc_mutex_lock( &p_fifo->lock );

    /* We do a while here because there is a race condition in the
     * win32 implementation of vlc_cond_wait() (We can't be sure the fifo
     * hasn't been emptied again since we were signaled). */
    while( p_fifo->p_first == NULL )
    {
        vlc_cond_wait( &p_fifo->wait, &p_fifo->lock );
    }

    b = p_fifo->p_first;

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

