/*****************************************************************************
 * cmd_input.cpp
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include <vlc/aout.h>
#include "cmd_input.hpp"
#include "cmd_dialogs.hpp"


void CmdPlay::execute()
{
    playlist_t *pPlaylist = getIntf()->p_sys->p_playlist;
    if( pPlaylist == NULL )
    {
        return;
    }

    if( pPlaylist->i_size )
    {
        playlist_Play( pPlaylist );
    }
    else
    {
        // If the playlist is empty, open a file requester instead
        CmdDlgFile cmd( getIntf() );
        cmd.execute();
    }
}


void CmdPause::execute()
{
    playlist_t *pPlaylist = getIntf()->p_sys->p_playlist;
    if( pPlaylist == NULL )
    {
        return;
    }

    playlist_Pause( pPlaylist );
}


void CmdStop::execute()
{
    playlist_t *pPlaylist = getIntf()->p_sys->p_playlist;
    if( pPlaylist == NULL )
    {
        return;
    }

    playlist_Stop( pPlaylist );
}


void CmdSlower::execute()
{
    input_thread_t *pInput =
        (input_thread_t *)vlc_object_find( getIntf(), VLC_OBJECT_INPUT,
                                           FIND_ANYWHERE );
    if( pInput )
    {
        vlc_value_t val;
        val.b_bool = VLC_TRUE;

        var_Set( pInput, "rate-slower", val );
        vlc_object_release( pInput );
    }
}


void CmdFaster::execute()
{
    input_thread_t *pInput =
        (input_thread_t *)vlc_object_find( getIntf(), VLC_OBJECT_INPUT,
                                           FIND_ANYWHERE );
    if( pInput )
    {
        vlc_value_t val;
        val.b_bool = VLC_TRUE;

        var_Set( pInput, "rate-faster", val );
        vlc_object_release( pInput );
    }
}


void CmdMute::execute()
{
    aout_VolumeMute( getIntf(), NULL );
}


void CmdVolumeUp::execute()
{
    aout_VolumeUp( getIntf(), 1, NULL );
}


void CmdVolumeDown::execute()
{
    aout_VolumeDown( getIntf(), 1, NULL );
}

