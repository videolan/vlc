/*****************************************************************************
 * cmd_playlist.cpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: cmd_playlist.cpp,v 1.2 2004/01/10 03:36:03 hartman Exp $
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

#include "cmd_playlist.hpp"


void CmdPlaylistDel::execute()
{
    m_rList.delSelected();
}


void CmdPlaylistSort::execute()
{
    // XXX add the mode and type
    playlist_t *pPlaylist = getIntf()->p_sys->p_playlist;
    if( pPlaylist != NULL )
    {
        playlist_Sort( pPlaylist, SORT_TITLE, ORDER_NORMAL );
    }

}


void CmdPlaylistNext::execute()
{
    playlist_t *pPlaylist = getIntf()->p_sys->p_playlist;
    if( pPlaylist != NULL )
    {
        playlist_Next( pPlaylist );
    }
}


void CmdPlaylistPrevious::execute()
{
    playlist_t *pPlaylist = getIntf()->p_sys->p_playlist;
    if( pPlaylist != NULL )
    {
        playlist_Prev( pPlaylist );
    }
}
