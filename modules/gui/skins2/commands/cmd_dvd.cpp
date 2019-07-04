/*****************************************************************************
 * cmd_dvd.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 *
 * Authors: Olivier Teulière <ipkiss@via.ecp.fr>
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

#include "cmd_dvd.hpp"
#include <vlc_player.h>
#include <vlc_playlist.h>

void CmdDvdNextTitle::execute()
{
    vlc_playlist_Lock( getPL() );
    vlc_player_t *player = vlc_playlist_GetPlayer( getPL() );
    vlc_player_SelectNextTitle( player );
    vlc_playlist_Unlock( getPL() );
}


void CmdDvdPreviousTitle::execute()
{
    vlc_playlist_Lock( getPL() );
    vlc_player_t *player = vlc_playlist_GetPlayer( getPL() );
    vlc_player_SelectPrevTitle( player );
    vlc_playlist_Unlock( getPL() );
}


void CmdDvdNextChapter::execute()
{
    vlc_playlist_Lock( getPL() );
    vlc_player_t *player = vlc_playlist_GetPlayer( getPL() );
    vlc_player_SelectNextChapter( player );
    vlc_playlist_Unlock( getPL() );
}


void CmdDvdPreviousChapter::execute()
{
    vlc_playlist_Lock( getPL() );
    vlc_player_t *player = vlc_playlist_GetPlayer( getPL() );
    vlc_player_SelectPrevChapter( player );
    vlc_playlist_Unlock( getPL() );
}


void CmdDvdRootMenu::execute()
{
    vlc_playlist_Lock( getPL() );
    vlc_player_t *player = vlc_playlist_GetPlayer( getPL() );
    vlc_player_Navigate( player, VLC_PLAYER_NAV_MENU );
    vlc_playlist_Unlock( getPL() );
}

