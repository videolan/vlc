/*****************************************************************************
 * stream_io_callback.cpp : matroska demuxer
 *****************************************************************************
 * Copyright (C) 2003-2004, 2010 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Steve Lhomme <steve.lhomme@free.fr>
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

#include "stream_io_callback.hpp"

namespace mkv {

/*****************************************************************************
 * Stream managment
 *****************************************************************************/
vlc_stream_io_callback::vlc_stream_io_callback( stream_t *s_, bool b_owner_ )
                       : s( s_), b_owner( b_owner_ )
{
    mb_eof = false;
}

uint32 vlc_stream_io_callback::read( void *p_buffer, size_t i_size )
{
    if( i_size <= 0 || mb_eof )
        return 0;

    int i_ret = vlc_stream_Read( s, p_buffer, i_size );
    return i_ret < 0 ? 0 : i_ret;
}

void vlc_stream_io_callback::setFilePointer(int64_t i_offset, seek_mode mode )
{
    int64_t i_pos, i_size;
    int64_t i_current = vlc_stream_Tell( s );

    switch( mode )
    {
        case seek_beginning:
            i_pos = i_offset;
            break;
        case seek_end:
            i_pos = stream_Size( s ) - i_offset;
            break;
        default:
            i_pos= i_current + i_offset;
            break;
    }

    if(i_pos == i_current)
    {
        if (mb_eof)
        {
            // if previous setFilePointer() failed we may be back in the available data
            i_size = stream_Size( s );
            if ( i_size != 0 && i_pos < i_size )
                mb_eof = vlc_stream_Seek( s, i_pos ) != VLC_SUCCESS;
        }
        return;
    }

    if( i_pos < 0 || ( ( i_size = stream_Size( s ) ) != 0 && i_pos >= i_size ) )
    {
        mb_eof = true;
        return;
    }

    mb_eof = false;
    if( vlc_stream_Seek( s, i_pos ) )
    {
        mb_eof = true;
    }
    return;
}

uint64 vlc_stream_io_callback::getFilePointer( void )
{
    if ( s == NULL )
        return 0;
    return vlc_stream_Tell( s );
}

size_t vlc_stream_io_callback::write(const void *, size_t )
{
    return 0;
}

uint64 vlc_stream_io_callback::toRead( void )
{
    uint64_t i_size;

    if( s == NULL)
        return 0;

    i_size = stream_Size( s );

    if( i_size <= 0 )
        return UINT64_MAX;

    return static_cast<uint64>( i_size - vlc_stream_Tell( s ) );
}

} // namespace
