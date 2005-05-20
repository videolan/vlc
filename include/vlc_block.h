/*****************************************************************************
 * vlc_block.h: Data blocks management functions
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#ifndef _VLC_BLOCK_H
#define _VLC_BLOCK_H 1

/****************************************************************************
 * block:
 ****************************************************************************
 * - block_sys_t is opaque and thus block_t->p_sys is PRIVATE
 * - i_flags may not always be set (ie could be 0, even for a key frame
 *      it depends where you receive the buffer (before/after a packetizer
 *      and the demux/packetizer implementations.
 * - i_dts/i_pts could be 0, it means no pts
 * - i_length: length in microseond of the packet, can be null except in the
 *      sout where it is mandatory.
 * - i_rate 0 or a valid input rate, look at vlc_input.h
 *
 * - i_buffer number of valid data pointed by p_buffer
 *      you can freely decrease it but never increase it yourself
 *      (use block_Realloc)
 * - p_buffer: pointer over datas. You should never overwrite it, you can
 *   only incremment it to skip datas, in others cases use block_Realloc
 *   (don't duplicate yourself in a bigger buffer, block_Realloc is
 *   optimised for prehader/postdatas increase)
 ****************************************************************************/
typedef struct block_sys_t block_sys_t;

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
/** Warm that this block is a header one */
#define BLOCK_FLAG_HEADER        0x0020
/** This is the last block of the frame */
#define BLOCK_FLAG_END_OF_FRAME  0x0040
/** This is not a key frame for bitrate shaping */
#define BLOCK_FLAG_NO_KEYFRAME   0x0080
/** This block contains a clock reference */
#define BLOCK_FLAG_CLOCK         0x0200
/** This block is scrambled */
#define BLOCK_FLAG_SCRAMBLED     0x0400
/** This block has to be decoded but not be displayed */
#define BLOCK_FLAG_PREROLL       0x0800
/** This block is corrupted and/or there is data loss  */
#define BLOCK_FLAG_CORRUPTED     0x1000

#define BLOCK_FLAG_PRIVATE_MASK  0xffff0000
#define BLOCK_FLAG_PRIVATE_SHIFT 16

struct block_t
{
    block_t     *p_next;

    uint32_t    i_flags;

    mtime_t     i_pts;
    mtime_t     i_dts;
    mtime_t     i_length;

    int         i_samples; /* Used for audio */
    int         i_rate;

    int         i_buffer;
    uint8_t     *p_buffer;

    /* This way the block_Release can be overloaded
     * Don't mess with it now, if you need it the ask on ML
     */
    void        (*pf_release)   ( block_t * );

    /* It's an object that should be valid as long as the block_t is valid */
    /* It should become a true block manager to reduce malloc/free */
    vlc_object_t    *p_manager;

    /* Following fields are private, user should never touch it */
    /* XXX never touch that OK !!! the first that access that will
     * have cvs account removed ;) XXX */
    block_sys_t *p_sys;
};

/****************************************************************************
 * Blocks functions:
 ****************************************************************************
 * - block_New : create a new block with the requested size ( >= 0 ), return
 *      NULL for failure.
 * - block_Release : release a block allocated with block_New.
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
#define block_New( a, b ) __block_New( VLC_OBJECT(a), b )
VLC_EXPORT( block_t *,  __block_New,        ( vlc_object_t *, int ) );
VLC_EXPORT( block_t *, block_Realloc,       ( block_t *, int i_pre, int i_body ) );

static inline block_t *block_Duplicate( block_t *p_block )
{
    block_t *p_dup = block_New( p_block->p_manager, p_block->i_buffer );

    p_dup->i_dts     = p_block->i_dts;
    p_dup->i_pts     = p_block->i_pts;
    p_dup->i_flags   = p_block->i_flags;
    p_dup->i_length  = p_block->i_length;
    p_dup->i_rate    = p_block->i_rate;
    p_dup->i_samples = p_block->i_samples;

    if( p_dup && p_block->i_buffer > 0 )
        memcpy( p_dup->p_buffer, p_block->p_buffer, p_block->i_buffer );

    return p_dup;
}
static inline void block_Release( block_t *p_block )
{
    p_block->pf_release( p_block );
}

/****************************************************************************
 * Chains of blocks functions helper
 ****************************************************************************
 * - block_ChainAppend : append a block the the last block of a chain. Try to
 *      avoid using with a lot of data as it's really slow, prefer
 *      block_ChainLastAppend
 * - block_ChainLastAppend : use a pointer over a pointer to the next blocks,
 *      and update it.
 * - block_ChainRelease : release a chain of block
 * - block_ChainExtract : extract data from a chain, return real bytes counts
 * - block_ChainGather : gather a chain, free it and return a block.
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

static inline void block_ChainLastAppend( block_t ***ppp_last, block_t *p_block  )
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
static int block_ChainExtract( block_t *p_list, void *p_data, int i_max )
{
    block_t *b;
    int     i_total = 0;
    uint8_t *p = (uint8_t*)p_data;

    for( b = p_list; b != NULL; b = b->p_next )
    {
        int i_copy = __MIN( i_max, b->i_buffer );
        if( i_copy > 0 )
        {
            memcpy( p, b->p_buffer, i_copy );
            i_max   -= i_copy;
            i_total += i_copy;
            p       += i_copy;

            if( i_max == 0 )
                return i_total;
        }
    }
    return i_total;
}

static inline block_t *block_ChainGather( block_t *p_list )
{
    int     i_total = 0;
    mtime_t i_length = 0;
    block_t *b, *g;

    if( p_list->p_next == NULL )
        return p_list;  /* Already gathered */

    for( b = p_list; b != NULL; b = b->p_next )
    {
        i_total += b->i_buffer;
        i_length += b->i_length;
    }

    g = block_New( p_list->p_manager, i_total );
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
 * Avoid touching block_fifo_t unless you really know what you are doing.
 * ( Some race conditions has to be correctly handled, like in win32 ;)
 * - block_FifoNew : create and init a new fifo
 * - block_FifoRelease : destroy a fifo and free all blocks in it.
 * - block_FifoEmpty : free all blocks in a fifo
 * - block_FifoPut : put a block
 * - block_FifoGet : get a packet from the fifo (and wait if it is empty)
 * - block_FifoShow : show the first packet of the fifo (and wait if
 *      needed), becarefull, you can use it ONLY if you are sure to be the
 *      only one getting data from the fifo.
 ****************************************************************************/
struct block_fifo_t
{
    vlc_mutex_t         lock;                         /* fifo data lock */
    vlc_cond_t          wait;         /* fifo data conditional variable */

    int                 i_depth;
    block_t             *p_first;
    block_t             **pp_last;
    int                 i_size;
};


#define block_FifoNew( a ) __block_FifoNew( VLC_OBJECT(a) )
VLC_EXPORT( block_fifo_t *, __block_FifoNew,    ( vlc_object_t * ) );
VLC_EXPORT( void,           block_FifoRelease,  ( block_fifo_t * ) );
VLC_EXPORT( void,           block_FifoEmpty,    ( block_fifo_t * ) );
VLC_EXPORT( int,            block_FifoPut,      ( block_fifo_t *, block_t * ) );
VLC_EXPORT( block_t *,      block_FifoGet,      ( block_fifo_t * ) );
VLC_EXPORT( block_t *,      block_FifoShow,     ( block_fifo_t * ) );

#endif /* VLC_BLOCK_H */
