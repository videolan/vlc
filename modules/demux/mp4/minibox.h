/*****************************************************************************
 * minibox.h: minimal mp4 box iterator
 *****************************************************************************
 * Copyright (C) 2017 VideoLabs, VLC authors and VideoLAN
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

typedef struct
{
    const uint8_t *p_buffer;
    size_t i_buffer;
    const uint8_t *p_payload;
    size_t i_payload;
    vlc_fourcc_t i_type;
} mp4_box_iterator_t;

static void mp4_box_iterator_Init( mp4_box_iterator_t *p_it,
                                   const uint8_t *p_data, size_t i_data )
{
    p_it->p_buffer = p_data;
    p_it->i_buffer = i_data;
}

static bool mp4_box_iterator_Next( mp4_box_iterator_t *p_it )
{
    while( p_it->i_buffer > 8 )
    {
        const uint8_t *p = p_it->p_buffer;
        const size_t i_size = GetDWBE( p );
        p_it->i_type = VLC_FOURCC(p[4], p[5], p[6], p[7]);
        if( i_size >= 8 && i_size <= p_it->i_buffer )
        {
            p_it->p_payload = &p_it->p_buffer[8];
            p_it->i_payload = i_size - 8;
            /* update for next run */
            p_it->p_buffer += i_size;
            p_it->i_buffer -= i_size;
            return true;
        }
        else break;
    }
    return false;
}
