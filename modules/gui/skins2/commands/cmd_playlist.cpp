/*****************************************************************************
 * cmd_playlist.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "cmd_playlist.hpp"
#include <vlc_playlist.h>
#include <vlc_url.h>
#include "../src/vlcproc.hpp"
#include "../utils/var_bool.hpp"

void CmdPlaylistDel::execute()
{
    m_rList.delSelected();
}

void CmdPlaylistNext::execute()
{
    playlist_Next( getPL() );
}


void CmdPlaylistPrevious::execute()
{
    playlist_Prev( getPL() );
}


void CmdPlaylistRandom::execute()
{
    var_SetBool( getPL(), "random", m_value );
}


void CmdPlaylistLoop::execute()
{
    var_SetBool( getPL(), "loop", m_value );
}


void CmdPlaylistRepeat::execute()
{
    var_SetBool( getPL(), "repeat", m_value );
}


void CmdPlaylistLoad::execute()
{
    char* psz_path = vlc_uri2path( m_file.c_str() );
    if ( !psz_path )
    {
        msg_Err(getIntf(),"unable to load playlist %s", m_file.c_str() );
        return;
    }
    playlist_Import( getPL(), psz_path );
    free( psz_path );
}


void CmdPlaylistSave::execute()
{
    const char *psz_module;
    if( m_file.find( ".xsp", 0 ) != std::string::npos )
        psz_module = "export-xspf";
    else if( m_file.find( "m3u", 0 ) != std::string::npos )
        psz_module = "export-m3u";
    else if( m_file.find( "html", 0 ) != std::string::npos )
        psz_module = "export-html";
    else
    {
        msg_Err(getIntf(),"Did not recognise playlist export file type");
        return;
    }

    playlist_Export( getPL(), m_file.c_str(), psz_module );
}

void CmdPlaylistFirst::execute()
{
    playlist_Control(getPL(), PLAYLIST_PLAY, pl_Unlocked);
}
