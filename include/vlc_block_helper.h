/*****************************************************************************
 * vlc_block_helper.h: Helper functions for data blocks management.
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: vlc_block_helper.h,v 1.1 2003/09/30 20:23:03 gbazin Exp $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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

#ifndef _VLC_BLOCK_HELPER_H
#define _VLC_BLOCK_HELPER_H 1

typedef struct block_bytestream_t
{
    block_t             *p_chain;
    block_t             *p_block;
    int                 i_offset;
} block_bytestream_t;

#define block_BytestreamInit( a, b, c ) __block_BytestreamInit( VLC_OBJECT(a), b, c )

/*****************************************************************************
 * block_bytestream_t management
 *****************************************************************************/
static inline block_bytestream_t __block_BytestreamInit( vlc_object_t *p_obj,
                                           block_t *p_block, int i_offset )
{
    block_bytestream_t bytestream;

    bytestream.i_offset = i_offset;
    bytestream.p_block = p_block;
    bytestream.p_chain = p_block;

    return bytestream;
}

static inline block_t *block_BytestreamFlush( block_bytestream_t *p_bytestream)
{
    while( p_bytestream->p_chain != p_bytestream->p_block )
    {
        block_t *p_next;
        p_next = p_bytestream->p_chain->p_next;
        p_bytestream->p_chain->pf_release( p_bytestream->p_chain );
        p_bytestream->p_chain = p_next;
    }

    return p_bytestream->p_chain;
}

static inline mtime_t block_BytestreamPTS( block_bytestream_t *p_bytestream )
{
    while( p_bytestream->p_chain != p_bytestream->p_block )
    {
        block_t *p_next;
        p_next = p_bytestream->p_chain->p_next;
        p_bytestream->p_chain->pf_release( p_bytestream->p_chain );
        p_bytestream->p_chain = p_next;
    }

    return p_bytestream->p_chain;
}

static inline int block_SkipByte( block_bytestream_t *p_bytestream )
{
    /* Most common case first */
    if( p_bytestream->p_block->i_buffer - p_bytestream->i_offset )
    {
        p_bytestream->i_offset++;
        return VLC_SUCCESS;
    }
    else
    {
        block_t *p_block;

        /* Less common case which is also slower */
        for( p_block = p_bytestream->p_block->p_next;
             p_block != NULL; p_block = p_block->p_next )
        {
            if( p_block->i_buffer )
            {
                p_bytestream->i_offset = 1;
                p_bytestream->p_block = p_block;
                return VLC_SUCCESS;
            }
        }
    }

    /* Not enough data, bail out */
    return VLC_EGENERIC;
}

static inline int block_PeekByte( block_bytestream_t *p_bytestream,
                                  uint8_t *p_data )
{
    /* Most common case first */
    if( p_bytestream->p_block->i_buffer - p_bytestream->i_offset )
    {
        *p_data = p_bytestream->p_block->p_buffer[p_bytestream->i_offset];
        return VLC_SUCCESS;
    }
    else
    {
        block_t *p_block;

        /* Less common case which is also slower */
        for( p_block = p_bytestream->p_block->p_next;
             p_block != NULL; p_block = p_block->p_next )
        {
            if( p_block->i_buffer )
            {
                *p_data = p_block->p_buffer[0];
                return VLC_SUCCESS;
            }
        }
    }

    /* Not enough data, bail out */
    return VLC_EGENERIC;
}

static inline int block_GetByte( block_bytestream_t *p_bytestream,
                                 uint8_t *p_data )
{
    /* Most common case first */
    if( p_bytestream->p_block->i_buffer - p_bytestream->i_offset )
    {
        *p_data = p_bytestream->p_block->p_buffer[p_bytestream->i_offset];
        p_bytestream->i_offset++;
        return VLC_SUCCESS;
    }
    else
    {
        block_t *p_block;

        /* Less common case which is also slower */
        for( p_block = p_bytestream->p_block->p_next;
             p_block != NULL; p_block = p_block->p_next )
        {
            if( p_block->i_buffer )
            {
                *p_data = p_block->p_buffer[0];
                p_bytestream->i_offset = 1;
                p_bytestream->p_block = p_block;
                return VLC_SUCCESS;
            }
        }
    }

    /* Not enough data, bail out */
    return VLC_EGENERIC;
}

static inline int block_SkipBytes( block_bytestream_t *p_bytestream,
                                   int i_data )
{
    block_t *p_block;
    int i_offset, i_copy;

    /* Check we have that much data */
    i_offset = p_bytestream->i_offset;
    i_copy = 0;
    for( p_block = p_bytestream->p_block;
         p_block != NULL; p_block = p_block->p_next )
    {
        i_copy = __MIN( i_data, p_block->i_buffer - i_offset );
        i_data -= i_copy;
        i_offset = 0;

        if( !i_data ) break;
    }

    if( i_data )
    {
        /* Not enough data, bail out */
        return VLC_EGENERIC;
    }

    p_bytestream->p_block = p_block;
    p_bytestream->i_offset = i_copy;
    return VLC_SUCCESS;
}

static inline int block_PeekBytes( block_bytestream_t *p_bytestream,
                                   uint8_t *p_data, int i_data )
{
    block_t *p_block;
    int i_offset, i_copy, i_size;

    /* Check we have that much data */
    i_offset = p_bytestream->i_offset;
    i_size = i_data;
    i_copy = 0;
    for( p_block = p_bytestream->p_block;
         p_block != NULL; p_block = p_block->p_next )
    {
        i_copy = __MIN( i_size, p_block->i_buffer - i_offset );
        i_size -= i_copy;
        i_offset = 0;

        if( !i_size ) break;
    }

    if( i_size )
    {
        /* Not enough data, bail out */
        return VLC_EGENERIC;
    }

    /* Copy the data */
    i_offset = p_bytestream->i_offset;
    i_size = i_data;
    i_copy = 0;
    for( p_block = p_bytestream->p_block;
         p_block != NULL; p_block = p_block->p_next )
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

static inline int block_GetBytes( block_bytestream_t *p_bytestream,
                                  uint8_t *p_data, int i_data )
{
    block_t *p_block;
    int i_offset, i_copy, i_size;

    /* Check we have that much data */
    i_offset = p_bytestream->i_offset;
    i_size = i_data;
    i_copy = 0;
    for( p_block = p_bytestream->p_block;
         p_block != NULL; p_block = p_block->p_next )
    {
        i_copy = __MIN( i_size, p_block->i_buffer - i_offset );
        i_size -= i_copy;
        i_offset = 0;

        if( !i_size ) break;
    }

    if( i_size )
    {
        /* Not enough data, bail out */
        return VLC_EGENERIC;
    }

    /* Copy the data */
    i_offset = p_bytestream->i_offset;
    i_size = i_data;
    i_copy = 0;
    for( p_block = p_bytestream->p_block;
         p_block != NULL; p_block = p_block->p_next )
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

    /* No buffer given, just skip the data */
    p_bytestream->p_block = p_block;
    p_bytestream->i_offset = i_copy;

    return VLC_SUCCESS;
}

static inline int block_PeekOffsetBytes( block_bytestream_t *p_bytestream,
    int i_peek_offset, uint8_t *p_data, int i_data )
{
    block_t *p_block;
    int i_offset, i_copy, i_size;

    /* Check we have that much data */
    i_offset = p_bytestream->i_offset;
    i_size = i_data + i_peek_offset;
    i_copy = 0;
    for( p_block = p_bytestream->p_block;
         p_block != NULL; p_block = p_block->p_next )
    {
        i_copy = __MIN( i_size, p_block->i_buffer - i_offset );
        i_size -= i_copy;
        i_offset = 0;

        if( !i_size ) break;
    }

    if( i_size )
    {
        /* Not enough data, bail out */
        return VLC_EGENERIC;
    }

    /* Find the right place */
    i_offset = p_bytestream->i_offset;
    i_size = i_peek_offset;
    i_copy = 0;
    for( p_block = p_bytestream->p_block;
         p_block != NULL; p_block = p_block->p_next )
    {
        i_copy = __MIN( i_size, p_block->i_buffer - i_offset );
        i_size -= i_copy;
        i_offset = 0;

        if( !i_size ) break;
    }

    /* Copy the data */
    i_offset = i_copy;
    i_size = i_data;
    i_copy = 0;
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

#endif /* VLC_BLOCK_HELPER_H */
