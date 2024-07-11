/*****************************************************************************
 * chapter_command.cpp : matroska demuxer
 *****************************************************************************
 * Copyright (C) 2003-2004 VLC authors and VideoLAN
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

#include "chapter_command.hpp"

#include <vlc_arrays.h>

#if LIBMATROSKA_VERSION < 0x010700
typedef enum {
  MATROSKA_CHAPPROCESSTIME_DURING           = 0,
  MATROSKA_CHAPPROCESSTIME_BEFORE           = 1,
  MATROSKA_CHAPPROCESSTIME_AFTER            = 2,
} MatroskaChapterProcessTime;
#endif

namespace mkv {

void chapter_codec_cmds_c::AddCommand( const KaxChapterProcessCommand & command )
{
    auto data = FindChild<KaxChapterProcessData>(command);
    if (unlikely(!data))
    {
        vlc_debug( l, "missing ChapProcessData" );
        return;
    }

    auto codec_time = FindChild<KaxChapterProcessTime>(command);
    if( unlikely(!codec_time) )
    {
        vlc_debug( l, "missing ChapProcessTime" );
        return;
    }
    if( static_cast<unsigned>(*codec_time) >= 3 )
    {
        vlc_debug( l, "unknown ChapProcessTime %d", static_cast<unsigned>(*codec_time) );
        return;
    }

    switch (static_cast<unsigned>(*codec_time))
    {
        case MATROSKA_CHAPPROCESSTIME_DURING: during_cmds.push_back( *data ); break;
        case MATROSKA_CHAPPROCESSTIME_BEFORE: enter_cmds.push_back( *data );  break;
        case MATROSKA_CHAPPROCESSTIME_AFTER:  leave_cmds.push_back( *data );  break;
        default: vlc_assert_unreachable();
    }
}

} // namespace
