/*****************************************************************************
 * block.c: Data blocks management functions
 *****************************************************************************
 * Copyright (C) 2003-2004 VideoLAN
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
#include <stdarg.h>

#include <vlc/vlc.h>
#include "vlc_block.h"

/* private */
struct block_sys_t
{
    vlc_mutex_t lock;

    uint8_t     *p_allocated_buffer;
    int         i_allocated_buffer;

    vlc_bool_t  b_modify;       /* has it been put in modified state */
    int         i_duplicated;   /* how many times has the content been
                                 * duplicated */

};

static void BlockRelease( block_t *p_block )
{
    vlc_mutex_lock( &p_block->p_sys->lock );

    p_block->p_sys->i_duplicated--;
    if( p_block->p_sys->i_duplicated < 0 )
    {
        vlc_mutex_unlock( &p_block->p_sys->lock );
        vlc_mutex_destroy( &p_block->p_sys->lock );
        free( p_block->p_sys->p_allocated_buffer );
        free( p_block->p_sys );
        free( p_block );

        return;
    }

    vlc_mutex_unlock( &p_block->p_sys->lock );
    free( p_block );
}

static block_t *__BlockDupContent( block_t *p_block )
{
    block_t *p_dup;

    p_dup = block_New( p_block->p_manager, p_block->i_buffer );
    memcpy( p_dup->p_buffer, p_block->p_buffer, p_block->i_buffer );
    p_dup->i_flags         = p_block->i_flags;
    p_dup->i_pts           = p_block->i_pts;
    p_dup->i_dts           = p_block->i_dts;
    p_dup->i_length        = p_block->i_length;
    p_dup->i_rate          = p_block->i_rate;

    return p_dup;
}

static block_t *BlockModify( block_t *p_block, vlc_bool_t b_will_modify )
{
    block_t *p_mod = p_block;   /* by default */

    vlc_mutex_lock( &p_block->p_sys->lock );

    if( p_block->p_sys->b_modify == b_will_modify )
    {
        vlc_mutex_unlock( &p_block->p_sys->lock );
        return p_block;
    }

    if( p_block->p_sys->i_duplicated == 0 )
    {
        p_block->p_sys->b_modify = b_will_modify;
        vlc_mutex_unlock( &p_block->p_sys->lock );
        return p_block;
    }

    /* FIXME we could avoid that
     * we just need to create a new p_sys with new mem FIXME */
    p_mod = __BlockDupContent( p_block );
    vlc_mutex_unlock( &p_block->p_sys->lock );

    BlockRelease( p_block );

    return p_mod;
}

static block_t *BlockDuplicate( block_t *p_block )
{
    block_t *p_dup;

    vlc_mutex_lock( &p_block->p_sys->lock );
    if( !p_block->p_sys->b_modify )
    {
        p_block->p_sys->i_duplicated++;
        vlc_mutex_unlock( &p_block->p_sys->lock );
        p_dup = block_NewEmpty();
        memcpy( p_dup, p_block, sizeof( block_t ) );
        p_dup->p_next = NULL;
        return p_dup;
    }
    p_dup = __BlockDupContent( p_block );
    vlc_mutex_unlock( &p_block->p_sys->lock );

    return p_dup;
}

static block_t *BlockRealloc( block_t *p_block, int i_prebody, int i_body )
{
    int i_buffer_size = i_prebody + i_body;

    if( i_body < 0 || i_buffer_size <= 0 ) return NULL;

    vlc_mutex_lock( &p_block->p_sys->lock );

    if( i_prebody < ( p_block->p_buffer - p_block->p_sys->p_allocated_buffer +
                      p_block->p_sys->i_allocated_buffer ) ||
        p_block->p_buffer - i_prebody > p_block->p_sys->p_allocated_buffer )
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

        p_rea->i_dts   = p_block->i_dts;
        p_rea->i_pts   = p_block->i_pts;
        p_rea->i_flags = p_block->i_flags;
        p_rea->i_length= p_block->i_length;
        p_rea->i_rate  = p_block->i_rate;

        memcpy( p_rea->p_buffer + i_prebody, p_block->p_buffer,
                __MIN( p_block->i_buffer, p_rea->i_buffer - i_prebody ) );

        vlc_mutex_unlock( &p_block->p_sys->lock );
        block_Release( p_block );

        return p_rea;
    }

    vlc_mutex_unlock( &p_block->p_sys->lock );

    return p_block;
}

/*****************************************************************************
 * Standard block management
 *
 *****************************************************************************/
/* to be used by other block management */
block_t *block_NewEmpty( void )
{
    block_t *p_block;

    p_block = malloc( sizeof( block_t ) );
    memset( p_block, 0, sizeof( block_t ) );

    p_block->p_next         = NULL;
    p_block->i_flags        = 0;
    p_block->i_pts          = 0;
    p_block->i_dts          = 0;
    p_block->i_length       = 0;
    p_block->i_rate         = 0;

    p_block->i_buffer       = 0;
    p_block->p_buffer       = NULL;

    p_block->pf_release     = NULL;
    p_block->pf_duplicate   = NULL;
    p_block->pf_modify      = NULL;
    p_block->pf_realloc     = NULL;

    p_block->p_manager      = NULL;
    p_block->p_sys = NULL;
    return p_block;
}

block_t *__block_New( vlc_object_t *p_obj, int i_size )
{
    block_t     *p_block;
    block_sys_t *p_sys;

    p_block = block_NewEmpty();

    p_block->pf_release     = BlockRelease;
    p_block->pf_duplicate   = BlockDuplicate;
    p_block->pf_modify      = BlockModify;
    p_block->pf_realloc     = BlockRealloc;

    /* that should be ok (no comunication between multiple p_vlc) */
    p_block->p_manager      = VLC_OBJECT( p_obj->p_vlc );

    p_block->p_sys = p_sys = malloc( sizeof( block_sys_t ) );
    vlc_mutex_init( p_obj, &p_sys->lock );

    /* XXX align on 16 and add 32 prebuffer/posbuffer bytes */
    p_sys->i_allocated_buffer = i_size + 32 + 32 + 16;
    p_sys->p_allocated_buffer = malloc( p_sys->i_allocated_buffer );
    p_block->i_buffer         = i_size;
    p_block->p_buffer         = &p_sys->p_allocated_buffer[32+15-((long)p_sys->p_allocated_buffer % 16 )];

    p_sys->i_duplicated = 0;
    p_sys->b_modify = VLC_TRUE;

    return p_block;
}

void block_ChainAppend( block_t **pp_list, block_t *p_block )
{
    if( *pp_list == NULL )
    {
        *pp_list = p_block;
    }
    else
    {
        block_t *p = *pp_list;

        while( p->p_next )
        {
            p = p->p_next;
        }
        p->p_next = p_block;
    }
}

void block_ChainLastAppend( block_t ***ppp_last, block_t *p_block  )
{
    block_t *p_last = p_block;

    /* Append the block */
    **ppp_last = p_block;

    /* Update last pointer */
    while( p_last->p_next ) p_last = p_last->p_next;
    *ppp_last = &p_last->p_next;
}

void block_ChainRelease( block_t *p_block )
{
    while( p_block )
    {
        block_t *p_next;
        p_next = p_block->p_next;
        p_block->pf_release( p_block );
        p_block = p_next;
    }
}

int block_ChainExtract( block_t *p_list, void *p_data, int i_max )
{
    block_t *b;
    int     i_total = 0;
    uint8_t *p = p_data;

    for( b = p_list; b != NULL; b = b->p_next )
    {
        int i_copy;

        i_copy = __MIN( i_max, b->i_buffer );
        if( i_copy > 0 )
        {
            memcpy( p, b->p_buffer, i_copy );
            i_max   -= i_copy;
            i_total += i_copy;
            p       += i_copy;

            if( i_max == 0 )
            {
                return i_total;
            }
        }
    }
    return i_total;
}

block_t *block_ChainGather( block_t *p_list )
{
    int     i_total = 0;
    block_t *b, *g;

    if( p_list->p_next == NULL )
    {
        /* only one, so no need */
        return p_list;
    }

    for( b = p_list; b != NULL; b = b->p_next )
    {
        i_total += b->i_buffer;
    }

    g = block_New( p_list->p_manager, i_total );
    block_ChainExtract( p_list, g->p_buffer, g->i_buffer );

    g->i_flags = p_list->i_flags;
    g->i_pts   = p_list->i_pts;
    g->i_dts   = p_list->i_dts;

    /* free p_list */
    block_ChainRelease( p_list );
    return g;
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
    p_fifo->i_depth = 0;
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

    p_fifo->i_depth = 0;
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

    if( p_fifo->p_first == NULL )
    {
        p_fifo->pp_last = &p_fifo->p_first;
    }

    vlc_mutex_unlock( &p_fifo->lock );

    b->p_next = NULL;
    return( b );
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

