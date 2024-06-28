// Copyright (C) 2003-2024 VLC authors and VideoLAN
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// chapter_command_script.cpp : Matroska Script Codec for Matroska Chapter Codecs
// Authors: Laurent Aimar <fenrir@via.ecp.fr>
//          Steve Lhomme <steve.lhomme@free.fr>


#include "chapter_command_script.hpp"
#include "virtual_segment.hpp"

namespace mkv {

//Matroska Script
const std::string matroska_script_interpretor_c::CMD_MS_GOTO_AND_PLAY = "GotoAndPlay";

// see http://www.matroska.org/technical/specs/chapters/index.html#mscript
//  for a description of existing commands
bool matroska_script_interpretor_c::Interpret( const binary * p_command, size_t i_size )
{
    bool b_result = false;

    std::string sz_command( reinterpret_cast<const char*> (p_command), i_size );

    vlc_debug( l, "command : %s", sz_command.c_str() );

    if ( sz_command.compare( 0, CMD_MS_GOTO_AND_PLAY.size(), CMD_MS_GOTO_AND_PLAY ) == 0 )
    {
        size_t i,j;

        // find the (
        for ( i=CMD_MS_GOTO_AND_PLAY.size(); i<sz_command.size(); i++)
        {
            if ( sz_command[i] == '(' )
            {
                i++;
                break;
            }
        }
        // find the )
        for ( j=i; j<sz_command.size(); j++)
        {
            if ( sz_command[j] == ')' )
            {
                i--;
                break;
            }
        }

        std::string st = sz_command.substr( i+1, j-i-1 );
        chapter_uid i_chapter_uid = std::stoul( st );

        virtual_segment_c *p_vsegment;
        virtual_chapter_c *p_vchapter = vm.FindVChapter( i_chapter_uid, p_vsegment );

        if ( p_vchapter == NULL )
            vlc_debug( l, "Chapter %" PRId64 " not found", i_chapter_uid);
        else
        {
            if ( !p_vchapter->EnterAndLeave( vm.GetCurrentVSegment()->CurrentChapter(), false ) )
                vm.JumpTo( *p_vsegment, *p_vchapter );
            b_result = true;
        }
    }

    return b_result;
}

} // namespace
