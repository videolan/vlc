/*****************************************************************************
 * vlcvars.cpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: vlcvars.cpp,v 1.1 2004/01/03 23:31:34 asmax Exp $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include "vlcvars.hpp"
#include <vlc/aout.h>


void VlcIsMute::set( bool value )
{
    VarBool::set( value );

    aout_VolumeMute( getIntf(), NULL );
}


void VlcIsPlaying::set( bool value, bool updateVLC )
{
    VarBool::set( value );

    if( !updateVLC )
    {
        return;
    }

    if( value )
    {
        //XXX use a command
        playlist_t *pPlaylist = getIntf()->p_sys->p_playlist;
        if( pPlaylist == NULL )
        {
            return;
        }
        playlist_Play( pPlaylist );
    }
    else
    {
        //XXX use a command
        playlist_t *pPlaylist = getIntf()->p_sys->p_playlist;
        if( pPlaylist == NULL )
        {
            return;
        }
        playlist_Pause( pPlaylist );
    }
}
