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

chapter_codec_cmds_c::~chapter_codec_cmds_c()
{
    delete p_private_data;
}

void chapter_codec_cmds_c::AddCommand( const KaxChapterProcessCommand & command )
{
    std::optional<MatroskaChapterProcessTime> codec_time;
    for( size_t i = 0; i < command.ListSize(); i++ )
    {
        if( MKV_CHECKED_PTR_DECL_CONST( p_cpt, KaxChapterProcessTime, command[i] ) )
        {
            codec_time = static_cast<MatroskaChapterProcessTime>( static_cast<unsigned>(*p_cpt) );
            break;
        }
    }

    if( !codec_time )
    {
        vlc_debug( l, "missing ChapProcessTime" );
        return;
    }
    if( *codec_time >= 3 )
    {
        vlc_debug( l, "unknown ChapProcessTime %d", *codec_time );
        return;
    }

    ChapterProcess *container;
    switch (*codec_time)
    {
        case MATROSKA_CHAPPROCESSTIME_DURING: container = &during_cmds; break;
        case MATROSKA_CHAPPROCESSTIME_BEFORE: container = &enter_cmds;  break;
        case MATROSKA_CHAPPROCESSTIME_AFTER:  container = &leave_cmds;  break;
        default: vlc_assert_unreachable();
    }

    for( size_t i = 0; i < command.ListSize(); i++ )
    {
        if( MKV_CHECKED_PTR_DECL_CONST( p_cpd, KaxChapterProcessData, command[i] ) )
        {
            container->push_back( *p_cpd );
        }
    }
}

} // namespace
