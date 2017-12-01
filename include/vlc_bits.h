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

/**
 * \file
 * This file defines functions, structures for handling streams of bits in vlc
 */

typedef struct bs_s
{
    uint8_t *p_start;
    uint8_t *p;
    uint8_t *p_end;

    ssize_t  i_left;    /* i_count number of available bits */
    bool     b_read_only;

     /* forward read modifier (p_start, p_end, p_fwpriv, count) */
    uint8_t *(*pf_forward)(uint8_t *, uint8_t *, void *, size_t);
    void    *p_fwpriv;
} bs_t;

static inline void bs_write_init( bs_t *s, void *p_data, size_t i_data )
{
    s->p_start = (uint8_t *)p_data;
    s->p       = s->p_start;
    s->p_end   = s->p_start + i_data;
    s->i_left  = 8;
    s->b_read_only = false;
    s->p_fwpriv = NULL;
    s->pf_forward = NULL;
}

static inline void bs_init( bs_t *s, const void *p_data, size_t i_data )
{
    bs_write_init( s, (void*) p_data, i_data );
    s->b_read_only = true;
}

static inline int bs_pos( const bs_t *s )
{
    return( 8 * ( s->p - s->p_start ) + 8 - s->i_left );
}

static inline int bs_remain( const bs_t *s )
{
    if( s->p >= s->p_end )
        return 0;
    else
    return( 8 * ( s->p_end - s->p ) - 8 + s->i_left );
}

static inline int bs_eof( const bs_t *s )
{
    return( s->p >= s->p_end ? 1: 0 );
}

#define bs_forward( s, i ) \
    s->p = s->pf_forward ? s->pf_forward( s->p, s->p_end, s->p_fwpriv, i ) : s->p + i

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
    int      i_shr, i_drop = 0;
    uint32_t i_result = 0;

    if( i_count > 32 )
    {
        i_drop = i_count - 32;
        i_count = 32;
    }

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
                bs_forward( s, 1 );
                s->i_left = 8;
            }
            break;
        }
        else
        {
            /* less in the buffer than requested */
           if( -i_shr == 32 )
               i_result = 0;
           else
               i_result |= (*s->p&i_mask[s->i_left]) << -i_shr;
           i_count  -= s->i_left;
           bs_forward( s, 1);
           s->i_left = 8;
        }
    }

    if( i_drop )
        bs_forward( s, i_drop );

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
            bs_forward( s, 1 );
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
        const size_t i_bytes = 1 + s->i_left / -8;
        bs_forward( s, i_bytes );
        if( i_bytes * 8 < i_bytes /* ofw */ )
            s->i_left = i_bytes;
        else
            s->i_left += 8 * i_bytes;
    }
}

static inline void bs_write( bs_t *s, int i_count, uint32_t i_bits )
{
    if( s->b_read_only )
        return;

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
            bs_forward( s, 1 );
            s->i_left = 8;
        }
    }
}

static inline bool bs_aligned( bs_t *s )
{
    return s->i_left % 8 == 0;
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
    while( !s->b_read_only && s->i_left != 8 )
    {
        bs_write( s, 1, 1 );
    }
}

/* Read unsigned Exp-Golomb code */
static inline uint_fast32_t bs_read_ue( bs_t * bs )
{
    unsigned i = 0;

    while( bs_read1( bs ) == 0 && bs->p < bs->p_end && i < 31 )
        i++;

    return (1U << i) - 1 + bs_read( bs, i );
}

/* Read signed Exp-Golomb code */
static inline int_fast32_t bs_read_se( bs_t *s )
{
    uint_fast32_t val = bs_read_ue( s );

    return (val & 0x01) ? (int_fast32_t)((val + 1) / 2)
                        : -(int_fast32_t)(val / 2);
}

#undef bs_forward

#endif
