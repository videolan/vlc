/*****************************************************************************
 * cmd_add_item.cpp
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_input_item.h>
#include <vlc_playlist.h>
#include <vlc_url.h>
#include "cmd_add_item.hpp"

void CmdAddItem::execute()
{
    if( strstr( m_name.c_str(), "://" ) == NULL )
    {
        char *psz_uri = vlc_path2uri( m_name.c_str(), NULL );
        if( !psz_uri )
            return;
        m_name = psz_uri;
        free( psz_uri );
    }
    input_item_t *media = input_item_New( m_name.c_str(), NULL );
    if( media )
    {
        vlc_playlist_Lock( getPL() );
        if( !vlc_playlist_AppendOne( getPL(), media ) && m_playNow )
        {
            ssize_t index = vlc_playlist_IndexOfMedia( getPL(), media );
            if( index != -1 )
                vlc_playlist_PlayAt( getPL(), index );
        }
        vlc_playlist_Unlock( getPL() );
        input_item_Release( media );
    }
}
