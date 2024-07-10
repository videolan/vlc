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

namespace mkv {

void chapter_codec_cmds_c::AddCommand( const KaxChapterProcessCommand & command )
{
    uint32_t codec_time = uint32_t(-1);
    for( size_t i = 0; i < command.ListSize(); i++ )
    {
        if( MKV_CHECKED_PTR_DECL_CONST( p_cpt, KaxChapterProcessTime, command[i] ) )
        {
            codec_time = static_cast<uint32_t>( *p_cpt );
            break;
        }
    }

    for( size_t i = 0; i < command.ListSize(); i++ )
    {
        if( MKV_CHECKED_PTR_DECL_CONST( p_cpd, KaxChapterProcessData, command[i] ) )
        {
            std::vector<KaxChapterProcessData*> *containers[] = {
                &during_cmds, /* codec_time = 0 */
                &enter_cmds,  /* codec_time = 1 */
                &leave_cmds   /* codec_time = 2 */
            };

            if( codec_time < 3 )
                containers[codec_time]->push_back( new KaxChapterProcessData( *p_cpd ) );
        }
    }
}

} // namespace
