/*****************************************************************************
 * subtitle.h: subtitle helper functions
 *****************************************************************************
 * Copyright (C) 2015 VLC authors and VideoLAN
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

inline static char * peek_Readline( stream_t *p_demuxstream, uint64_t *pi_offset )
{
    uint8_t *p_peek;
    ssize_t i_peek = vlc_stream_Peek( p_demuxstream, (const uint8_t **) &p_peek,
                                  *pi_offset + 2048 );
    if( i_peek < 0 || (uint64_t) i_peek < *pi_offset )
        return NULL;

    const uint64_t i_bufsize = (uint64_t) i_peek - *pi_offset;
    char *psz_line = NULL;

    /* Create a stream memory from that offset */
    stream_t *p_memorystream = vlc_stream_MemoryNew( p_demuxstream,
                                                     &p_peek[*pi_offset],
                                                     i_bufsize, true );
    if( p_memorystream )
    {
        psz_line = vlc_stream_ReadLine( p_memorystream );

        *pi_offset += vlc_stream_Tell( p_memorystream );
        vlc_stream_Delete( p_memorystream );
    }

    return psz_line;
}
