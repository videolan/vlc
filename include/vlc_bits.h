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

#include <vlc_common.h>
#include <vlc_block.h>

/**
 * \file
 * This file defines functions, structures for handling streams of bits in vlc
 */

typedef struct bo_t
{
    block_t     *b;
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
    s->p_start = (uint8_t *)p_data;
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

static inline bool bo_init(bo_t *p_bo, int i_size)
{
    p_bo->b = block_Alloc(i_size);
    if (p_bo->b == NULL)
        return false;

    p_bo->b->i_buffer = 0;
    p_bo->basesize = i_size;

    return true;
}

static inline void bo_deinit(bo_t *p_bo)
{
    if(p_bo->b)
        block_Release(p_bo->b);
}

static inline void bo_free(bo_t *p_bo)
{
    if(!p_bo)
        return;
    bo_deinit(p_bo);
    free(p_bo);
}

static inline int bo_extend(bo_t *p_bo, size_t i_total)
{
    if(!p_bo->b)
        return false;
    const size_t i_size = p_bo->b->i_size - (p_bo->b->p_buffer - p_bo->b->p_start);
    if (i_total >= i_size)
    {
        int i_growth = p_bo->basesize;
        while(i_total >= i_size + i_growth)
            i_growth += p_bo->basesize;

        int i = p_bo->b->i_buffer; /* Realloc would set payload size == buffer size */
        p_bo->b = block_Realloc(p_bo->b, 0, i_size + i_growth);
        if (!p_bo->b)
            return false;
        p_bo->b->i_buffer = i;
    }
    return true;
}

#define BO_SET_DECL_S(func, handler, type) static inline bool func(bo_t *p_bo, size_t i_offset, type val)\
    {\
        if (!bo_extend(p_bo, i_offset + sizeof(type)))\
            return false;\
        handler(&p_bo->b->p_buffer[i_offset], val);\
        return true;\
    }

#define BO_ADD_DECL_S(func, handler, type) static inline bool func(bo_t *p_bo, type val)\
    {\
        if(!p_bo->b || !handler(p_bo, p_bo->b->i_buffer, val))\
            return false;\
        p_bo->b->i_buffer += sizeof(type);\
        return true;\
    }

#define BO_FUNC_DECL(suffix, handler, type ) \
    BO_SET_DECL_S( bo_set_ ## suffix ## be, handler ## BE, type )\
    BO_SET_DECL_S( bo_set_ ## suffix ## le, handler ## LE, type )\
    BO_ADD_DECL_S( bo_add_ ## suffix ## be, bo_set_ ## suffix ## be, type )\
    BO_ADD_DECL_S( bo_add_ ## suffix ## le, bo_set_ ## suffix ## le, type )

static inline bool bo_set_8(bo_t *p_bo, size_t i_offset, uint8_t i)
{
    if (!bo_extend(p_bo, i_offset + 1))
        return false;
    p_bo->b->p_buffer[i_offset] = i;
    return true;
}

static inline bool bo_add_8(bo_t *p_bo, uint8_t i)
{
    if(!p_bo->b || !bo_set_8( p_bo, p_bo->b->i_buffer, i ))
        return false;
    p_bo->b->i_buffer++;
    return true;
}

/* declares all bo_[set,add]_[16,32,64] */
BO_FUNC_DECL( 16, SetW,  uint16_t )
BO_FUNC_DECL( 32, SetDW, uint32_t )
BO_FUNC_DECL( 64, SetQW, uint64_t )

#undef BO_FUNC_DECL
#undef BO_SET_DECL_S
#undef BO_ADD_DECL_S

static inline bool bo_add_24be(bo_t *p_bo, uint32_t i)
{
    if(!p_bo->b || !bo_extend(p_bo, p_bo->b->i_buffer + 3))
        return false;
    p_bo->b->p_buffer[p_bo->b->i_buffer++] = ((i >> 16) & 0xff);
    p_bo->b->p_buffer[p_bo->b->i_buffer++] = ((i >> 8) & 0xff);
    p_bo->b->p_buffer[p_bo->b->i_buffer++] = (i & 0xff);
    return true;
}

static inline void bo_swap_32be (bo_t *p_bo, size_t i_pos, uint32_t i)
{
    if (!p_bo->b || p_bo->b->i_buffer < i_pos + 4)
        return;
    p_bo->b->p_buffer[i_pos    ] = (i >> 24)&0xff;
    p_bo->b->p_buffer[i_pos + 1] = (i >> 16)&0xff;
    p_bo->b->p_buffer[i_pos + 2] = (i >>  8)&0xff;
    p_bo->b->p_buffer[i_pos + 3] = (i      )&0xff;
}

static inline bool bo_add_mem(bo_t *p_bo, size_t i_size, const void *p_mem)
{
    if(!p_bo->b || !bo_extend(p_bo, p_bo->b->i_buffer + i_size))
        return false;
    memcpy(&p_bo->b->p_buffer[p_bo->b->i_buffer], p_mem, i_size);
    p_bo->b->i_buffer += i_size;
    return true;
}

#define bo_add_fourcc(p_bo, fcc) bo_add_mem(p_bo, 4, fcc)

#endif
