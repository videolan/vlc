/*****************************************************************************
 * vlc_bits.h : Bit handling helpers
 *****************************************************************************
 * Copyright (C) 2001, 2002, 2003, 2006, 2015 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin at videolan dot org>
 *          Rafaël Carré <funman at videolan dot org>
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

#ifndef VLC_BITS_H
#define VLC_BITS_H 1

#include <vlc_block.h>

/**
 * \file
 * This file defines functions, structures for handling streams of bits in vlc
 */

typedef struct bo_t
{
    block_t     *b;
    size_t      len;
    size_t      basesize;
} bo_t;

typedef struct bs_s
{
    uint8_t *p_start;
    uint8_t *p;
    uint8_t *p_end;

    ssize_t  i_left;    /* i_count number of available bits */
} bs_t;

static inline void bs_init( bs_t *s, const void *p_data, size_t i_data )
{
    s->p_start = (void *)p_data;
    s->p       = s->p_start;
    s->p_end   = s->p_start + i_data;
    s->i_left  = 8;
}

static inline int bs_pos( const bs_t *s )
{
    return( 8 * ( s->p - s->p_start ) + 8 - s->i_left );
}

static inline int bs_eof( const bs_t *s )
{
    return( s->p >= s->p_end ? 1: 0 );
}

static inline uint32_t bs_read( bs_t *s, int i_count )
{
     static const uint32_t i_mask[33] =
     {  0x00,
        0x01,      0x03,      0x07,      0x0f,
        0x1f,      0x3f,      0x7f,      0xff,
        0x1ff,     0x3ff,     0x7ff,     0xfff,
        0x1fff,    0x3fff,    0x7fff,    0xffff,
        0x1ffff,   0x3ffff,   0x7ffff,   0xfffff,
        0x1fffff,  0x3fffff,  0x7fffff,  0xffffff,
        0x1ffffff, 0x3ffffff, 0x7ffffff, 0xfffffff,
        0x1fffffff,0x3fffffff,0x7fffffff,0xffffffff};
    int      i_shr;
    uint32_t i_result = 0;

    while( i_count > 0 )
    {
        if( s->p >= s->p_end )
        {
            break;
        }

        if( ( i_shr = s->i_left - i_count ) >= 0 )
        {
            /* more in the buffer than requested */
            i_result |= ( *s->p >> i_shr )&i_mask[i_count];
            s->i_left -= i_count;
            if( s->i_left == 0 )
            {
                s->p++;
                s->i_left = 8;
            }
            return( i_result );
        }
        else
        {
            /* less in the buffer than requested */
           i_result |= (*s->p&i_mask[s->i_left]) << -i_shr;
           i_count  -= s->i_left;
           s->p++;
           s->i_left = 8;
        }
    }

    return( i_result );
}

static inline uint32_t bs_read1( bs_t *s )
{
    if( s->p < s->p_end )
    {
        unsigned int i_result;

        s->i_left--;
        i_result = ( *s->p >> s->i_left )&0x01;
        if( s->i_left == 0 )
        {
            s->p++;
            s->i_left = 8;
        }
        return i_result;
    }

    return 0;
}

static inline uint32_t bs_show( bs_t *s, int i_count )
{
    bs_t     s_tmp = *s;
    return bs_read( &s_tmp, i_count );
}

static inline void bs_skip( bs_t *s, ssize_t i_count )
{
    s->i_left -= i_count;

    if( s->i_left <= 0 )
    {
        const int i_bytes = ( -s->i_left + 8 ) / 8;

        s->p += i_bytes;
        s->i_left += 8 * i_bytes;
    }
}

static inline void bs_write( bs_t *s, int i_count, uint32_t i_bits )
{
    while( i_count > 0 )
    {
        if( s->p >= s->p_end )
        {
            break;
        }

        i_count--;

        if( ( i_bits >> i_count )&0x01 )
        {
            *s->p |= 1 << ( s->i_left - 1 );
        }
        else
        {
            *s->p &= ~( 1 << ( s->i_left - 1 ) );
        }
        s->i_left--;
        if( s->i_left == 0 )
        {
            s->p++;
            s->i_left = 8;
        }
    }
}

static inline void bs_align( bs_t *s )
{
    if( s->i_left != 8 )
    {
        s->i_left = 8;
        s->p++;
    }
}

static inline void bs_align_0( bs_t *s )
{
    if( s->i_left != 8 )
    {
        bs_write( s, s->i_left, 0 );
    }
}

static inline void bs_align_1( bs_t *s )
{
    while( s->i_left != 8 )
    {
        bs_write( s, 1, 1 );
    }
}

static inline void bo_init(bo_t *p_bo, int i_size)
{
    p_bo->b = block_Alloc(i_size);
    p_bo->b->i_buffer = 0;
    p_bo->len = p_bo->basesize = i_size;
}

static inline void bo_set_8(bo_t *p_bo, size_t i_offset, uint8_t i)
{
    if (i_offset >= p_bo->len)
    {
        int i_growth = p_bo->basesize;
        while(i_offset >= p_bo->len + i_growth)
            i_growth += p_bo->basesize;

        int i = p_bo->b->i_buffer; /* Realloc would set payload size == buffer size */
        p_bo->b = block_Realloc(p_bo->b, 0, p_bo->len + i_growth);
        if (!p_bo->b)
            return;
        p_bo->b->i_buffer = i;
        p_bo->len += i_growth;
    }
    p_bo->b->p_buffer[i_offset] = i;
}

static inline void bo_add_8(bo_t *p_bo, uint8_t i)
{
    bo_set_8( p_bo, p_bo->b->i_buffer, i );
    p_bo->b->i_buffer++;
}

static inline void bo_add_16be(bo_t *p_bo, uint16_t i)
{
    bo_add_8(p_bo, ((i >> 8) &0xff));
    bo_add_8(p_bo, i &0xff);
}

static inline void bo_add_16le(bo_t *p_bo, uint16_t i)
{
    bo_add_8(p_bo, i &0xff);
    bo_add_8(p_bo, ((i >> 8) &0xff));
}

static inline void bo_set_16be(bo_t *p_bo, int i_offset, uint16_t i)
{
    bo_set_8(p_bo, i_offset + 1, ((i >> 8) &0xff));
    bo_set_8(p_bo, i_offset, i &0xff);
}

static inline void bo_set_16le(bo_t *p_bo, int i_offset, uint16_t i)
{
    bo_set_8(p_bo, i_offset, i &0xff);
    bo_set_8(p_bo, i_offset + 1, ((i >> 8) &0xff));
}

static inline void bo_add_24be(bo_t *p_bo, uint32_t i)
{
    bo_add_8(p_bo, ((i >> 16) &0xff));
    bo_add_8(p_bo, ((i >> 8) &0xff));
    bo_add_8(p_bo, (i &0xff));
}

static inline void bo_add_32be(bo_t *p_bo, uint32_t i)
{
    bo_add_16be(p_bo, ((i >> 16) &0xffff));
    bo_add_16be(p_bo, i &0xffff);
}

static inline void bo_add_32le(bo_t *p_bo, uint32_t i)
{
    bo_add_16le(p_bo, i &0xffff);
    bo_add_16le(p_bo, ((i >> 16) &0xffff));
}

static inline void bo_set_32be(bo_t *p_bo, int i_offset, uint32_t i)
{
    bo_set_16be(p_bo, i_offset + 2, ((i >> 16) &0xffff));
    bo_set_16be(p_bo, i_offset, i &0xffff);
}

static inline void bo_set_32le(bo_t *p_bo, int i_offset, uint32_t i)
{
    bo_set_16le(p_bo, i_offset, i &0xffff);
    bo_set_16le(p_bo, i_offset + 2, ((i >> 16) &0xffff));
}

static inline void bo_swap_32be (bo_t *p_bo, int i_pos, uint32_t i)
{
    p_bo->b->p_buffer[i_pos    ] = (i >> 24)&0xff;
    p_bo->b->p_buffer[i_pos + 1] = (i >> 16)&0xff;
    p_bo->b->p_buffer[i_pos + 2] = (i >>  8)&0xff;
    p_bo->b->p_buffer[i_pos + 3] = (i      )&0xff;
}

static inline void bo_add_64be(bo_t *p_bo, uint64_t i)
{
    bo_add_32be(p_bo, ((i >> 32) &0xffffffff));
    bo_add_32be(p_bo, i &0xffffffff);
}

static inline void bo_add_64le(bo_t *p_bo, uint64_t i)
{
    bo_add_32le(p_bo, i &0xffffffff);
    bo_add_32le(p_bo, ((i >> 32) &0xffffffff));
}

static inline void bo_add_fourcc(bo_t *p_bo, const char *fcc)
{
    bo_add_8(p_bo, fcc[0]);
    bo_add_8(p_bo, fcc[1]);
    bo_add_8(p_bo, fcc[2]);
    bo_add_8(p_bo, fcc[3]);
}

static inline void bo_add_mem(bo_t *p_bo, int i_size, const uint8_t *p_mem)
{
    for (int i = 0; i < i_size; i++)
        bo_add_8(p_bo, p_mem[i]);
}

static inline void bo_add_mp4_tag_descr(bo_t *p_bo, uint8_t tag, uint32_t size)
{
    bo_add_8(p_bo, tag);
    for (int i = 3; i>0; i--)
        bo_add_8(p_bo, (size>>(7*i)) | 0x80);
    bo_add_8(p_bo, size & 0x7F);
}

#endif
