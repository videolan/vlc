/*****************************************************************************
 * playlist.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
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

#include <vlc/vlc.h>

#include "playlist.hpp"
#include "../utils/ustring.hpp"

Playlist::Playlist( intf_thread_t *pIntf ): VarList( pIntf )
{
    // Get the playlist VLC object
    m_pPlaylist = pIntf->p_sys->p_playlist;

    buildList();
}


Playlist::~Playlist()
{
}


void Playlist::delSelected()
{
    // Remove the items from the VLC playlist
    int index = 0;
    ConstIterator it;
    for( it = begin(); it != end(); it++ )
    {
        if( (*it).m_selected )
        {
            playlist_item_t *p_item = playlist_LockItemGetByPos( m_pPlaylist,
                                                                 index );
            playlist_LockDelete( m_pPlaylist, p_item->input.i_id );
        }
        else
        {
            index++;
        }
    }

    notify();
}


void Playlist::action( Elem_t *pItem )
{
    // Find the index of the item
    int index = 0;
    ConstIterator it;
    for( it = begin(); it != end(); it++ )
    {
        if( &*it == pItem ) break;
        index++;
    }
    // Item found ?
    if( index < size() )
    {
        playlist_Goto( m_pPlaylist, index );
    }
}


void Playlist::onChange()
{
    buildList();
    notify();
}


void Playlist::buildList()
{
    clear();

    vlc_mutex_lock( &m_pPlaylist->object_lock );
    for( int i = 0; i < m_pPlaylist->i_size; i++ )
    {
        // Get the name of the playlist item
        UString *pName =
            new UString( getIntf(), m_pPlaylist->pp_items[i]->input.psz_name );
        // Is it the played stream ?
        bool playing = (i == m_pPlaylist->i_index );
        // Add the item in the list
        m_list.push_back( Elem_t( UStringPtr( pName ), false, playing ) );
    }
    vlc_mutex_unlock( &m_pPlaylist->object_lock );
}

