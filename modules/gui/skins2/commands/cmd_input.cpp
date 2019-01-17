/*****************************************************************************
 * cmd_input.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "cmd_input.hpp"
#include "cmd_dialogs.hpp"
#include <vlc_input.h>
#include <vlc_playlist_legacy.h>

void CmdPlay::execute()
{
    playlist_t *pPlaylist = getPL();

    // if already playing an input, reset rate to normal speed
    input_thread_t *pInput = playlist_CurrentInput( pPlaylist );
    if( pInput )
    {
        var_SetFloat( getPL(), "rate", 1.0 );
        vlc_object_release( pInput );
    }

    playlist_Lock( pPlaylist );
    const bool b_empty = playlist_IsEmpty( pPlaylist );
    playlist_Unlock( pPlaylist );

    if( !b_empty )
    {
        playlist_Play( pPlaylist );
    }
    else
    {
        // If the playlist is empty, open a file requester instead
        CmdDlgFile( getIntf() ).execute();
    }
}


void CmdPause::execute()
{
    playlist_TogglePause( getPL() );
}


void CmdStop::execute()
{
    playlist_Stop( getPL() );
}


void CmdSlower::execute()
{
    var_TriggerCallback( getPL(), "rate-slower" );
}


void CmdFaster::execute()
{
    var_TriggerCallback( getPL(), "rate-faster" );
}


void CmdMute::execute()
{
    playlist_MuteToggle( getPL() );
}


void CmdVolumeUp::execute()
{
    playlist_VolumeUp( getPL(), 1, NULL );
}


void CmdVolumeDown::execute()
{
    playlist_VolumeDown( getPL(), 1, NULL );
}

