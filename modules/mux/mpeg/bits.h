/*****************************************************************************
 * bits.h
 *****************************************************************************
 * Copyright (C) 2001, 2002 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

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
        p_buffer->i_data++;
        p_buffer->p_data[p_buffer->i_data] = 0x00;
    }
}

static inline void bits_write( bits_buffer_t *p_buffer,
                               int i_count, uint64_t i_bits )
{
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


