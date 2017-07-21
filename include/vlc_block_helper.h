/*****************************************************************************
 * vlc_block_helper.h: Helper functions for data blocks management.
 *****************************************************************************
 * Copyright (C) 2003-2017 VLC authors and VideoLAN
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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

#ifndef VLC_BLOCK_HELPER_H
#define VLC_BLOCK_HELPER_H 1

#include <vlc_block.h>

typedef struct block_bytestream_t
{
    block_t *p_chain;  /**< byte stream head block */
    block_t **pp_last; /**< tail ppointer for appends */
    block_t *p_block;  /**< byte stream read pointer block */
    size_t   i_block_offset; /**< byte stream read pointer offset within block */
    size_t   i_base_offset; /**< block base offset (previous blocks total size) */
    size_t   i_total;  /**< total bytes over all linked blocks */
} block_bytestream_t;

/*****************************************************************************
 * block_bytestream_t management
 *****************************************************************************/
static inline void block_BytestreamInit( block_bytestream_t *p_bytestream )
{
    p_bytestream->p_chain = p_bytestream->p_block = NULL;
    p_bytestream->pp_last = &p_bytestream->p_chain;
    p_bytestream->i_block_offset = 0;
    p_bytestream->i_base_offset = 0;
    p_bytestream->i_total = 0;
}

static inline void block_BytestreamRelease( block_bytestream_t *p_bytestream )
{
    block_ChainRelease( p_bytestream->p_chain );
}

/**
 * It flush all data (read and unread) from a block_bytestream_t.
 */
static inline void block_BytestreamEmpty( block_bytestream_t *p_bytestream )
{
    block_BytestreamRelease( p_bytestream );
    block_BytestreamInit( p_bytestream );
}

/**
 * It flushes all already read data from a block_bytestream_t.
 */
static inline void block_BytestreamFlush( block_bytestream_t *p_bytestream )
{
    block_t *block = p_bytestream->p_chain;

    while( block != p_bytestream->p_block )
    {
        block_t *p_next = block->p_next;

        p_bytestream->i_total -= block->i_buffer;
        p_bytestream->i_base_offset -= block->i_buffer;
        block_Release( block );
        block = p_next;
    }

    while( block != NULL && block->i_buffer == p_bytestream->i_block_offset )
    {
        block_t *p_next = block->p_next;

        p_bytestream->i_total -= block->i_buffer;
        block_Release( block );
        block = p_next;
        p_bytestream->i_block_offset = 0;
    }

    p_bytestream->p_chain = p_bytestream->p_block = block;
    if( p_bytestream->p_chain == NULL )
        p_bytestream->pp_last = &p_bytestream->p_chain;
}

static inline void block_BytestreamPush( block_bytestream_t *p_bytestream,
                                         block_t *p_block )
{
    block_ChainLastAppend( &p_bytestream->pp_last, p_block );
    if( !p_bytestream->p_block ) p_bytestream->p_block = p_block;
    for( ; p_block; p_block = p_block->p_next )
        p_bytestream->i_total += p_block->i_buffer;
}

static inline size_t block_BytestreamRemaining( const block_bytestream_t *p_bytestream )
{
    return ( p_bytestream->i_total > p_bytestream->i_base_offset + p_bytestream->i_block_offset ) ?
             p_bytestream->i_total - p_bytestream->i_base_offset - p_bytestream->i_block_offset : 0;
}

VLC_USED
static inline block_t *block_BytestreamPop( block_bytestream_t *p_bytestream )
{
    block_t *p_block;

    block_BytestreamFlush( p_bytestream );

    p_block = p_bytestream->p_block;
    if( unlikely( p_block == NULL ) )
    {
        return NULL;
    }
    else if( !p_block->p_next )
    {
        p_block->p_buffer += p_bytestream->i_block_offset;
        p_block->i_buffer -= p_bytestream->i_block_offset;
        p_bytestream->i_block_offset = 0;
        p_bytestream->i_total = 0;
        p_bytestream->p_chain = p_bytestream->p_block = NULL;
        p_bytestream->pp_last = &p_bytestream->p_chain;
        return p_block;
    }

    while( p_block->p_next && p_block->p_next->p_next )
        p_block = p_block->p_next;

    block_t *p_block_old = p_block;
    p_block = p_block->p_next;
    p_block_old->p_next = NULL;
    p_bytestream->pp_last = &p_block_old->p_next;
    if( p_block )
        p_bytestream->i_total -= p_block->i_buffer;

    return p_block;
}

static inline int block_WaitBytes( block_bytestream_t *p_bytestream,
                                   size_t i_data )
{
    if( block_BytestreamRemaining( p_bytestream ) >= i_data )
        return VLC_SUCCESS;
    return VLC_EGENERIC;
}

static inline int block_PeekBytes( block_bytestream_t *p_bytestream,
                                   uint8_t *p_data, size_t i_data )
{
    if( block_BytestreamRemaining( p_bytestream ) < i_data )
        return VLC_EGENERIC;

    /* Copy the data */
    size_t i_offset = p_bytestream->i_block_offset;
    size_t i_size = i_data;
    for( block_t *p_block = p_bytestream->p_block;
         p_block != NULL; p_block = p_block->p_next )
    {
        size_t i_copy = __MIN( i_size, p_block->i_buffer - i_offset );
        i_size -= i_copy;

        if( i_copy )
        {
            memcpy( p_data, p_block->p_buffer + i_offset, i_copy );
            p_data += i_copy;
        }

        i_offset = 0;

        if( !i_size ) break;
    }

    return VLC_SUCCESS;
}

static inline int block_GetBytes( block_bytestream_t *p_bytestream,
                                  uint8_t *p_data, size_t i_data )
{
    if( block_BytestreamRemaining( p_bytestream ) < i_data )
        return VLC_EGENERIC;

    /* Copy the data */
    size_t i_offset = p_bytestream->i_block_offset;
    size_t i_size = i_data;
    size_t i_copy = 0;
    block_t *p_block;
    for( p_block = p_bytestream->p_block;
         p_block != NULL; p_block = p_block->p_next )
    {
        i_copy = __MIN( i_size, p_block->i_buffer - i_offset );
        i_size -= i_copy;

        if( i_copy && p_data != NULL )
        {
            memcpy( p_data, p_block->p_buffer + i_offset, i_copy );
            p_data += i_copy;
        }

        if( i_size == 0 )
            break;

        p_bytestream->i_base_offset += p_block->i_buffer;
        i_offset = 0;
    }

    p_bytestream->p_block = p_block;
    p_bytestream->i_block_offset = i_offset + i_copy;

    return VLC_SUCCESS;
}

static inline int block_SkipBytes( block_bytestream_t *p_bytestream,
                                   size_t i_data )
{
    return block_GetBytes( p_bytestream, NULL, i_data );
}

static inline int block_SkipByte( block_bytestream_t *p_bytestream )
{
    return block_GetBytes( p_bytestream, NULL, 1 );
}

static inline int block_PeekOffsetBytes( block_bytestream_t *p_bytestream,
    size_t i_peek_offset, uint8_t *p_data, size_t i_data )
{
    const size_t i_remain = block_BytestreamRemaining( p_bytestream );
    if( i_remain < i_data + i_peek_offset )
        return VLC_EGENERIC;

    /* Find the right place */
    size_t i_offset = p_bytestream->i_block_offset;
    size_t i_size = i_peek_offset;
    size_t i_copy = 0;
    block_t *p_block;
    for( p_block = p_bytestream->p_block;
         p_block != NULL; p_block = p_block->p_next )
    {
        i_copy = __MIN( i_size, p_block->i_buffer - i_offset );
        i_size -= i_copy;

        if( !i_size ) break;

        i_offset = 0;
    }

    /* Copy the data */
    i_offset += i_copy;
    i_size = i_data;
    for( ; p_block != NULL; p_block = p_block->p_next )
    {
        i_copy = __MIN( i_size, p_block->i_buffer - i_offset );
        i_size -= i_copy;

        if( i_copy )
        {
            memcpy( p_data, p_block->p_buffer + i_offset, i_copy );
            p_data += i_copy;
        }

        i_offset = 0;

        if( !i_size ) break;
    }

    return VLC_SUCCESS;
}

typedef const uint8_t * (*block_startcode_helper_t)( const uint8_t *, const uint8_t * );
typedef bool (*block_startcode_matcher_t)( uint8_t, size_t, const uint8_t * );

static inline int block_FindStartcodeFromOffset(
    block_bytestream_t *p_bytestream, size_t *pi_offset,
    const uint8_t *p_startcode, int i_startcode_length,
    block_startcode_helper_t p_startcode_helper,
    block_startcode_matcher_t p_startcode_matcher )
{
    block_t *p_block, *p_block_backup = 0;
    ssize_t i_size = 0;
    size_t i_offset, i_offset_backup = 0;
    int i_caller_offset_backup = 0, i_match;

    /* Find the right place */
    i_size = *pi_offset + p_bytestream->i_block_offset;
    for( p_block = p_bytestream->p_block;
         p_block != NULL; p_block = p_block->p_next )
    {
        i_size -= p_block->i_buffer;
        if( i_size < 0 ) break;
    }

    if( unlikely( i_size >= 0 ) )
    {
        /* Not enough data, bail out */
        return VLC_EGENERIC;
    }

    /* Begin the search.
     * We first look for an occurrence of the 1st startcode byte and
     * if found, we do a more thorough check. */
    i_size += p_block->i_buffer;
    *pi_offset -= i_size;
    i_match = 0;
    for( ; p_block != NULL; p_block = p_block->p_next )
    {
        for( i_offset = i_size; i_offset < p_block->i_buffer; i_offset++ )
        {
            /* Use optimized helper when possible */
            if( p_startcode_helper && !i_match &&
               (p_block->i_buffer - i_offset) > ((size_t)i_startcode_length - 1) )
            {
                const uint8_t *p_res = p_startcode_helper( &p_block->p_buffer[i_offset],
                                                           &p_block->p_buffer[p_block->i_buffer] );
                if( p_res )
                {
                    *pi_offset += i_offset + (p_res - &p_block->p_buffer[i_offset]);
                    return VLC_SUCCESS;
                }
                /* Then parsing boundary with legacy code */
                i_offset = p_block->i_buffer - (i_startcode_length - 1);
            }

            bool b_matched = ( p_startcode_matcher )
                           ? p_startcode_matcher( p_block->p_buffer[i_offset], i_match, p_startcode )
                           : p_block->p_buffer[i_offset] == p_startcode[i_match];
            if( b_matched )
            {
                if( i_match == 0 )
                {
                    p_block_backup = p_block;
                    i_offset_backup = i_offset;
                    i_caller_offset_backup = *pi_offset;
                }

                if( i_match + 1 == i_startcode_length )
                {
                    /* We have it */
                    *pi_offset += i_offset - i_match;
                    return VLC_SUCCESS;
                }

                i_match++;
            }
            else if ( i_match > 0 )
            {
                /* False positive */
                p_block = p_block_backup;
                i_offset = i_offset_backup;
                *pi_offset = i_caller_offset_backup;
                i_match = 0;
            }

        }
        i_size = 0;
        *pi_offset += i_offset;
    }

    *pi_offset -= i_match;
    return VLC_EGENERIC;
}

#endif /* VLC_BLOCK_HELPER_H */
