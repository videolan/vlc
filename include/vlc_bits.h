/*****************************************************************************
 * vlc_bits.h : Bit handling helpers
 *****************************************************************************
 * Copyright (C) 2001, 2002, 2003, 2006, 2015 VLC authors and VideoLAN
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

typedef struct bs_s bs_t;

typedef struct
{
    /* forward read modifier (p_start, p_end, p_fwpriv, count, pos) */
    size_t (*pf_byte_forward)(bs_t *, size_t);
    size_t (*pf_byte_pos)(const bs_t *);
} bs_byte_callbacks_t;

typedef struct bs_s
{
    uint8_t *p_start;
    uint8_t *p;         /* points to currently read/written byte */
    uint8_t *p_end;

    uint8_t  i_left;    /* i_count number of available bits */
    bool     b_read_only;
    bool     b_error;

    bs_byte_callbacks_t cb;
    void    *p_priv;
} bs_t;

static size_t bs_impl_bytes_forward( bs_t *s, size_t i_count )
{
    if( s->p == NULL )
    {
        s->p = s->p_start;
        return 1;
    }

    if( s->p >= s->p_end )
        return 0;

    if( (size_t) (s->p_end - s->p) < i_count )
        i_count = s->p_end - s->p;
    s->p += i_count;
    return i_count;
}

static size_t bs_impl_bytes_pos( const bs_t *s )
{
    if( s->p )
        return s->p < s->p_end ? s->p - s->p_start + 1 : s->p - s->p_start;
    else
        return 0;
}

static inline void bs_init_custom( bs_t *s, const void *p_data, size_t i_data,
                                   const bs_byte_callbacks_t *cb, void *priv )
{
    s->p_start = (uint8_t *)p_data;
    s->p       = NULL;
    s->p_end   = s->p_start + i_data;
    s->i_left  = 0;
    s->b_read_only = true;
    s->b_error = false;
    s->p_priv = priv;
    s->cb = *cb;
}

static inline void bs_init( bs_t *s, const void *p_data, size_t i_data )
{
    bs_byte_callbacks_t cb = {
        bs_impl_bytes_forward,
        bs_impl_bytes_pos,
    };
    bs_init_custom( s, p_data, i_data, &cb, NULL );
}

static inline void bs_write_init( bs_t *s, void *p_data, size_t i_data )
{
    bs_init( s, (const void *) p_data, i_data );
    s->b_read_only = false;
}

static inline int bs_refill( bs_t *s )
{
    if( s->i_left == 0 )
    {
       if( s->cb.pf_byte_forward( s, 1 ) != 1 )
           return -1;

       if( s->p < s->p_end )
            s->i_left = 8;
    }
    return s->i_left > 0 ? 0 : 1;
}

static inline bool bs_error( const bs_t *s )
{
    return s->b_error;
}

static inline bool bs_eof( bs_t *s )
{
    return bs_refill( s ) != 0;
}

static inline size_t bs_pos( const bs_t *s )
{
    return 8 * s->cb.pf_byte_pos( s ) - s->i_left;
}

static inline void bs_skip( bs_t *s, size_t i_count )
{
    if( i_count == 0 )
        return;

    if( bs_refill( s ) )
    {
        s->b_error = true;
        return;
    }

    if( i_count > s->i_left )
    {
        i_count -= s->i_left;
        s->i_left = 0;
        size_t bytes = i_count / 8;
        if( bytes && s->cb.pf_byte_forward( s, bytes ) != bytes )
        {
            s->b_error = true;
            return;
        }
        i_count = i_count % 8;
        if( i_count > 0 )
        {
            if( !bs_refill( s ) )
                s->i_left = 8 - i_count;
            else
                s->b_error = true;
        }
    }
    else s->i_left -= i_count;
}

static inline uint32_t bs_read( bs_t *s, uint8_t i_count )
{
    uint8_t  i_shr, i_drop = 0;
    uint32_t i_result = 0;

    if( i_count > 32 )
    {
        i_drop = i_count - 32;
        i_count = 32;
    }

    while( i_count > 0 )
    {
        if( bs_refill( s ) )
        {
            s->b_error = true;
            break;
        }

        if( s->i_left > i_count )
        {
            uint_fast32_t mask = (UINT64_C(1) << i_count) - 1;

            i_shr = s->i_left - i_count;
            /* more in the buffer than requested */
            i_result |= ( *s->p >> i_shr ) & mask;
            s->i_left -= i_count;
            break;
        }
        else
        {
            uint_fast32_t mask = (UINT64_C(1) << s->i_left) - 1;

            i_shr = i_count - s->i_left;
            /* less in the buffer than requested */
            if( i_shr >= 32 )
                i_result = 0;
            else
                i_result |= (*s->p & mask) << i_shr;
            i_count  -= s->i_left;
            s->i_left = 0;
        }
    }

    if( i_drop )
        bs_skip( s, i_drop );

    return( i_result );
}

static inline uint32_t bs_read1( bs_t *s )
{
    if( bs_refill( s ) )
    {
        s->b_error = true;
        return 0;
    }
    s->i_left--;
    return ( *s->p >> s->i_left )&0x01;
}

static inline void bs_write( bs_t *s, uint8_t i_count, uint32_t i_bits )
{
    if( s->b_read_only )
        return;

    while( i_count > 0 )
    {
        if( bs_refill( s ) )
        {
            s->b_error = true;
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
    }
}

static inline bool bs_aligned( bs_t *s )
{
    return s->i_left % 8 == 0;
}

static inline void bs_align( bs_t *s )
{
    if( s->i_left % 8 )
        s->i_left = 0;
}

static inline void bs_write_align( bs_t *s, uint8_t v )
{
    if( !s->b_read_only && (s->i_left % 8) )
        bs_write( s, s->i_left, v ? 0xFF : 0 );
}

#define bs_align_0( s ) bs_write_align( s, 0 )
#define bs_align_1( s ) bs_write_align( s, 1 )

/* Read unsigned Exp-Golomb code */
static inline uint_fast32_t bs_read_ue( bs_t * bs )
{
    unsigned i = 0;

    while( !bs->b_error &&
           bs_read1( bs ) == 0 &&
           bs->p < bs->p_end && i < 31 )
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
