/*****************************************************************************
 * cmd_add_item.cpp
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_playlist.h>
#include <vlc_url.h>
#include "cmd_add_item.hpp"

void CmdAddItem::execute()
{
    playlist_t *pPlaylist = getIntf()->p_sys->p_playlist;
    if( !pPlaylist )
        return;

    if( strstr( m_name.c_str(), "://" ) == NULL )
    {
        char *psz_uri = vlc_path2uri( m_name.c_str(), NULL );
        if( !psz_uri )
            return;
        m_name = psz_uri;
        free( psz_uri );
    }
    playlist_Add( pPlaylist, m_name.c_str(), NULL,
                  m_playNow ? PLAYLIST_APPEND | PLAYLIST_GO : PLAYLIST_APPEND,
                  PLAYLIST_END, true, false );
}
