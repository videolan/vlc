// Copyright (C) 2024 VLC authors and VideoLAN
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// chapter_command_script_common.cpp : 
// Common file for Matroska JS and Matroska Script
// Authors: Laurent Aimar <fenrir@via.ecp.fr>
//          Steve Lhomme <steve.lhomme@free.fr>
//          Khalid Masum <khalid.masum.92@gmail.com>


#include "chapter_command_script_common.hpp"

namespace mkv {

bool matroska_script_codec_common_c::Enter()
{
    bool f_result = false;
    ChapterProcess::iterator index = enter_cmds.begin();
    while ( index != enter_cmds.end() )
    {
        if ( (*index).GetSize() )
        {
            vlc_debug( l, "Matroska Script enter command" );
            f_result |= get_interpreter().Interpret( (*index).GetBuffer(), (*index).GetSize() );
        }
        ++index;
    }
    return f_result;
}

bool matroska_script_codec_common_c::Leave()
{
    bool f_result = false;
    ChapterProcess::iterator index = leave_cmds.begin();
    while ( index != leave_cmds.end() )
    {
        if ( (*index).GetSize() )
        {
            vlc_debug( l, "Matroska Script leave command" );
            f_result |= get_interpreter().Interpret( (*index).GetBuffer(), (*index).GetSize() );
        }
        ++index;
    }
    return f_result;
}

} // namespace
