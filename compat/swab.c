/*****************************************************************************
 * swab.c: swab() replacement
 *****************************************************************************
 * Copyright © 2009 VLC authors and VideoLAN
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <stdint.h>

void swab( const void *p_src_, void *p_dst_, ssize_t n )
{
    const uint8_t *p_src = p_src_;
    uint8_t *p_dst = p_dst_;

    if( n < 0 )
        return;

    for( ssize_t i = 0; i < n-1; i += 2 )
    {
        uint8_t i_tmp = p_src[i+0];
        p_dst[i+0] = p_src[i+1];
        p_dst[i+1] = i_tmp;
    }
}

