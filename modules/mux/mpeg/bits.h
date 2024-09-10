/*****************************************************************************
 * bits.h
 *****************************************************************************
 * Copyright (C) 2001, 2002 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
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

#include <limits.h>
#include <stdbit.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct bits_buffer_s
{
    int     i_size;

    int     i_data;
    uint8_t i_mask;
    uint8_t *p_data;

} bits_buffer_t;

static inline int bits_initwrite( bits_buffer_t *p_buffer,
                                  int i_size, void *p_data )
{
    p_buffer->i_size = i_size;
    p_buffer->i_data = 0;
    p_buffer->i_mask = 0x80;
    p_buffer->p_data = p_data;
    if( !p_buffer->p_data )
    {
        if( !( p_buffer->p_data = malloc( i_size ) ) )
            return -1;
    }
    p_buffer->p_data[0] = 0;
    return 0;
}

static inline void bits_align( bits_buffer_t *p_buffer )
{
    if( p_buffer->i_mask != 0x80 && p_buffer->i_data < p_buffer->i_size )
    {
        p_buffer->i_mask = 0x80;
        if( ++p_buffer->i_data < p_buffer->i_size )
            p_buffer->p_data[p_buffer->i_data] = 0x00;
    }
}

static inline bool bits_will_overflow( const bits_buffer_t *buff, int count )
{
    const int leftover = count - (stdc_trailing_zeros(buff->i_mask) + 1);
    const int byte_shift = (leftover + CHAR_BIT - 1) / CHAR_BIT;
    return buff->i_data + byte_shift >= buff->i_size;
}

static inline void bits_write( bits_buffer_t *p_buffer,
                               int i_count, uint64_t i_bits )
{
    if( bits_will_overflow(p_buffer, i_count) )
        return;

    while( i_count > 0 )
    {
        i_count--;

        if( ( i_bits >> i_count )&0x01 )
        {
            p_buffer->p_data[p_buffer->i_data] |= p_buffer->i_mask;
        }
        else
        {
            p_buffer->p_data[p_buffer->i_data] &= ~p_buffer->i_mask;
        }
        p_buffer->i_mask >>= 1;
        if( p_buffer->i_mask == 0 )
        {
            p_buffer->i_data++;
            p_buffer->i_mask = 0x80;
        }
    }
}


