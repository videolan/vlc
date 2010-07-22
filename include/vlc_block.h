/*****************************************************************************
 * vlc_block.h: Data blocks management functions
 *****************************************************************************
 * Copyright (C) 2003 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#ifndef VLC_BLOCK_H
#define VLC_BLOCK_H 1

/**
 * \file
 * This file implements functions and structures to handle blocks of data in vlc
 *
 */

#include <sys/types.h>  /* for ssize_t */

/****************************************************************************
 * block:
 ****************************************************************************
 * - i_flags may not always be set (ie could be 0, even for a key frame
 *      it depends where you receive the buffer (before/after a packetizer
 *      and the demux/packetizer implementations.
 * - i_dts/i_pts could be VLC_TS_INVALID, it means no pts/dts
 * - i_length: length in microseond of the packet, can be null except in the
 *      sout where it is mandatory.
 *
 * - i_buffer number of valid data pointed by p_buffer
 *      you can freely decrease it but never increase it yourself
 *      (use block_Realloc)
 * - p_buffer: pointer over datas. You should never overwrite it, you can
 *   only incremment it to skip datas, in others cases use block_Realloc
 *   (don't duplicate yourself in a bigger buffer, block_Realloc is
 *   optimised for preheader/postdatas increase)
 ****************************************************************************/

/** The content doesn't follow the last block, or is probably broken */
#define BLOCK_FLAG_DISCONTINUITY 0x0001
/** Intra frame */
#define BLOCK_FLAG_TYPE_I        0x0002
/** Inter frame with backward reference only */
#define BLOCK_FLAG_TYPE_P        0x0004
/** Inter frame with backward and forward reference */
#define BLOCK_FLAG_TYPE_B        0x0008
/** For inter frame when you don't know the real type */
#define BLOCK_FLAG_TYPE_PB       0x0010
/** Warn that this block is a header one */
#define BLOCK_FLAG_HEADER        0x0020
/** This is the last block of the frame */
#define BLOCK_FLAG_END_OF_FRAME  0x0040
/** This is not a key frame for bitrate shaping */
#define BLOCK_FLAG_NO_KEYFRAME   0x0080
/** This block contains the last part of a sequence  */
#define BLOCK_FLAG_END_OF_SEQUENCE 0x0100
/** This block contains a clock reference */
#define BLOCK_FLAG_CLOCK         0x0200
/** This block is scrambled */
#define BLOCK_FLAG_SCRAMBLED     0x0400
/** This block has to be decoded but not be displayed */
#define BLOCK_FLAG_PREROLL       0x0800
/** This block is corrupted and/or there is data loss  */
#define BLOCK_FLAG_CORRUPTED     0x1000
/** This block contains an interlaced picture with top field first */
#define BLOCK_FLAG_TOP_FIELD_FIRST 0x2000
/** This block contains an interlaced picture with bottom field first */
#define BLOCK_FLAG_BOTTOM_FIELD_FIRST 0x4000

/** This block contains an interlaced picture */
#define BLOCK_FLAG_INTERLACED_MASK \
    (BLOCK_FLAG_TOP_FIELD_FIRST|BLOCK_FLAG_BOTTOM_FIELD_FIRST)

#define BLOCK_FLAG_TYPE_MASK \
    (BLOCK_FLAG_TYPE_I|BLOCK_FLAG_TYPE_P|BLOCK_FLAG_TYPE_B|BLOCK_FLAG_TYPE_PB)

/* These are for input core private usage only */
#define BLOCK_FLAG_CORE_PRIVATE_MASK  0x00ff0000
#define BLOCK_FLAG_CORE_PRIVATE_SHIFT 16

/* These are for module private usage only */
#define BLOCK_FLAG_PRIVATE_MASK  0xff000000
#define BLOCK_FLAG_PRIVATE_SHIFT 24

typedef void (*block_free_t) (block_t *);

struct block_t
{
    block_t    *p_next;

    uint8_t    *p_buffer; /**< Payload start */
    size_t      i_buffer; /**< Payload length */
    uint8_t    *p_start; /**< Buffer start */
    size_t      i_size; /**< Buffer total size */

    uint32_t    i_flags;
    unsigned    i_nb_samples; /* Used for audio */

    mtime_t     i_pts;
    mtime_t     i_dts;
    mtime_t     i_length;

    /* Rudimentary support for overloading block (de)allocation. */
    block_free_t pf_release;
};

/****************************************************************************
 * Blocks functions:
 ****************************************************************************
 * - block_Alloc : create a new block with the requested size ( >= 0 ), return
 *      NULL for failure.
 * - block_Release : release a block allocated with block_Alloc.
 * - block_Realloc : realloc a block,
 *      i_pre: how many bytes to insert before body if > 0, else how many
 *      bytes of body to skip (the latter can be done without using
 *      block_Realloc i_buffer -= -i_pre, p_buffer += -i_pre as i_pre < 0)
 *      i_body (>= 0): the final size of the body (decreasing it can directly
 *      be done with i_buffer = i_body).
 *      with preheader and or body (increase
 *      and decrease are supported). Use it as it is optimised.
 * - block_Duplicate : create a copy of a block.
 ****************************************************************************/
VLC_API void block_Init( block_t *, void *, size_t );
VLC_API block_t *block_Alloc( size_t ) VLC_USED VLC_MALLOC;
VLC_API block_t *block_Realloc( block_t *, ssize_t i_pre, size_t i_body ) VLC_USED;

static inline void block_CopyProperties( block_t *dst, block_t *src )
{
    dst->i_flags   = src->i_flags;
    dst->i_nb_samples = src->i_nb_samples;
    dst->i_dts     = src->i_dts;
    dst->i_pts     = src->i_pts;
    dst->i_length  = src->i_length;
}

VLC_USED
static inline block_t *block_Duplicate( block_t *p_block )
{
    block_t *p_dup = block_Alloc( p_block->i_buffer );
    if( p_dup == NULL )
        return NULL;

    block_CopyProperties( p_dup, p_block );
    memcpy( p_dup->p_buffer, p_block->p_buffer, p_block->i_buffer );

    return p_dup;
}

static inline void block_Release( block_t *p_block )
{
    p_block->pf_release( p_block );
}

VLC_API block_t *block_heap_Alloc(void *, size_t) VLC_USED VLC_MALLOC;
VLC_API block_t *block_mmap_Alloc(void *addr, size_t length) VLC_USED VLC_MALLOC;
VLC_API block_t * block_shm_Alloc(void *addr, size_t length) VLC_USED VLC_MALLOC;
VLC_API block_t *block_File(int fd) VLC_USED VLC_MALLOC;
VLC_API block_t *block_FilePath(const char *) VLC_USED VLC_MALLOC;

static inline void block_Cleanup (void *block)
{
    block_Release ((block_t *)block);
}
#define block_cleanup_push( block ) vlc_cleanup_push (block_Cleanup, block)

/****************************************************************************
 * Chains of blocks functions helper
 ****************************************************************************
 * - block_ChainAppend : append a block to the last block of a chain. Try to
 *      avoid using with a lot of data as it's really slow, prefer
 *      block_ChainLastAppend, p_block can be NULL
 * - block_ChainLastAppend : use a pointer over a pointer to the next blocks,
 *      and update it.
 * - block_ChainRelease : release a chain of block
 * - block_ChainExtract : extract data from a chain, return real bytes counts
 * - block_ChainGather : gather a chain, free it and return one block.
 ****************************************************************************/
static inline void block_ChainAppend( block_t **pp_list, block_t *p_block )
{
    if( *pp_list == NULL )
    {
        *pp_list = p_block;
    }
    else
    {
        block_t *p = *pp_list;

        while( p->p_next ) p = p->p_next;
        p->p_next = p_block;
    }
}

static inline void block_ChainLastAppend( block_t ***ppp_last, block_t *p_block )
{
    block_t *p_last = p_block;

    **ppp_last = p_block;

    while( p_last->p_next ) p_last = p_last->p_next;
    *ppp_last = &p_last->p_next;
}

static inline void block_ChainRelease( block_t *p_block )
{
    while( p_block )
    {
        block_t *p_next = p_block->p_next;
        block_Release( p_block );
        p_block = p_next;
    }
}

static size_t block_ChainExtract( block_t *p_list, void *p_data, size_t i_max )
{
    size_t  i_total = 0;
    uint8_t *p = (uint8_t*)p_data;

    while( p_list && i_max )
    {
        size_t i_copy = __MIN( i_max, p_list->i_buffer );
        memcpy( p, p_list->p_buffer, i_copy );
        i_max   -= i_copy;
        i_total += i_copy;
        p       += i_copy;

        p_list = p_list->p_next;
    }
    return i_total;
}

static inline void block_ChainProperties( block_t *p_list, int *pi_count, size_t *pi_size, mtime_t *pi_length )
{
    size_t i_size = 0;
    mtime_t i_length = 0;
    int i_count = 0;

    while( p_list )
    {
        i_size += p_list->i_buffer;
        i_length += p_list->i_length;
        i_count++;

        p_list = p_list->p_next;
    }

    if( pi_size )
        *pi_size = i_size;
    if( pi_length )
        *pi_length = i_length;
    if( pi_count )
        *pi_count = i_count;
}

static inline block_t *block_ChainGather( block_t *p_list )
{
    size_t  i_total = 0;
    mtime_t i_length = 0;
    block_t *g;

    if( p_list->p_next == NULL )
        return p_list;  /* Already gathered */

    block_ChainProperties( p_list, NULL, &i_total, &i_length );

    g = block_Alloc( i_total );
    block_ChainExtract( p_list, g->p_buffer, g->i_buffer );

    g->i_flags = p_list->i_flags;
    g->i_pts   = p_list->i_pts;
    g->i_dts   = p_list->i_dts;
    g->i_length = i_length;

    /* free p_list */
    block_ChainRelease( p_list );
    return g;
}

/****************************************************************************
 * Fifos of blocks.
 ****************************************************************************
 * - block_FifoNew : create and init a new fifo
 * - block_FifoRelease : destroy a fifo and free all blocks in it.
 * - block_FifoPace : wait for a fifo to drain to a specified number of packets or total data size
 * - block_FifoEmpty : free all blocks in a fifo
 * - block_FifoPut : put a block
 * - block_FifoGet : get a packet from the fifo (and wait if it is empty)
 * - block_FifoShow : show the first packet of the fifo (and wait if
 *      needed), be carefull, you can use it ONLY if you are sure to be the
 *      only one getting data from the fifo.
 * - block_FifoCount : how many packets are waiting in the fifo
 *
 * block_FifoGet and block_FifoShow are cancellation points.
 ****************************************************************************/

VLC_API block_fifo_t *block_FifoNew( void ) VLC_USED VLC_MALLOC;
VLC_API void block_FifoRelease( block_fifo_t * );
VLC_API void block_FifoPace( block_fifo_t *fifo, size_t max_depth, size_t max_size );
VLC_API void block_FifoEmpty( block_fifo_t * );
VLC_API size_t block_FifoPut( block_fifo_t *, block_t * );
void block_FifoWake( block_fifo_t * );
VLC_API block_t * block_FifoGet( block_fifo_t * ) VLC_USED;
VLC_API block_t * block_FifoShow( block_fifo_t * );
size_t block_FifoSize( const block_fifo_t *p_fifo ) VLC_USED;
VLC_API size_t block_FifoCount( const block_fifo_t *p_fifo ) VLC_USED;

#endif /* VLC_BLOCK_H */
