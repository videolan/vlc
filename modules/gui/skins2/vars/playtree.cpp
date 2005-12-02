/*****************************************************************************
 * playtree.cpp
 *****************************************************************************
 * Copyright (C) 2005 VideoLAN
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea@videolan.org>
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

#include "playtree.hpp"
#include "../utils/ustring.hpp"

#include "charset.h"


Playtree::Playtree( intf_thread_t *pIntf ): VarTree( pIntf )
{
    // Get the VLC playlist object
    m_pPlaylist = pIntf->p_sys->p_playlist;

    // Try to guess the current charset
    char *pCharset;
    vlc_current_charset( &pCharset );
    iconvHandle = vlc_iconv_open( "UTF-8", pCharset );
    msg_Dbg( pIntf, "Using character encoding: %s", pCharset );
    free( pCharset );

    if( iconvHandle == (vlc_iconv_t) - 1 )
    {
        msg_Warn( pIntf, "Unable to do requested conversion" );
    }

    buildTree();
}

Playtree::~Playtree()
{
    if( iconvHandle != (vlc_iconv_t) - 1 ) vlc_iconv_close( iconvHandle );
    // TODO : check that everything is destroyed
}

void Playtree::delSelected()
{
    // TODO
    notify();
}

void Playtree::action( VarTree *pItem )
{
    vlc_mutex_lock( &m_pPlaylist->object_lock );
    VarTree::Iterator it;
    if( pItem->size() )
    {
        it = pItem->begin();
        while( it->size() ) it = it->begin();
    }
    playlist_Control( m_pPlaylist,
                      PLAYLIST_VIEWPLAY,
                      m_pPlaylist->status.i_view,
                      pItem->size()
                          ? (playlist_item_t *)pItem->m_pData
                          : (playlist_item_t *)pItem->parent()->m_pData,
                      pItem->size()
                          ? (playlist_item_t *)it->m_pData
                          : (playlist_item_t *)pItem->m_pData
                    );
    vlc_mutex_unlock( &m_pPlaylist->object_lock );
}

void Playtree::onChange()
{
    buildTree();
    notify();
}

void Playtree::onUpdate( int id )
{
    Iterator it = findById( id );
    if( it != end() )
    {
        // Update the item
        playlist_item_t* pNode = (playlist_item_t*)(it->m_pData);
        UString *pName = new UString( getIntf(), pNode->input.psz_name );
        it->m_cString = UStringPtr( pName );
        it->m_playing = m_pPlaylist->status.p_item == pNode;
    }
    else
    {
        msg_Warn(getIntf(), "Cannot find node with id %d", id );
    }
    // TODO update only the right node
    notify();
}

void Playtree::buildNode( playlist_item_t *pNode, VarTree &rTree )
{
    for( int i = 0; i < pNode->i_children; i++ )
    {
        UString *pName = new UString( getIntf(),
                                      pNode->pp_children[i]->input.psz_name );
        rTree.add( pNode->pp_children[i]->input.i_id, UStringPtr( pName ),
                     false,
                     m_pPlaylist->status.p_item == pNode->pp_children[i],
                     true, pNode->pp_children[i] );
        if( pNode->pp_children[i]->i_children )
        {
            buildNode( pNode->pp_children[i], rTree.back() );
        }
    }
}

void Playtree::buildTree()
{
    clear();
    vlc_mutex_lock( &m_pPlaylist->object_lock );

    playlist_view_t *p_view;
    p_view = playlist_ViewFind( m_pPlaylist, VIEW_CATEGORY );
    /* TODO : let the user chose the view type */

    clear();
    /* XXX : do we need Playlist::clear() instead of VarTree::clear() ? */

    /* Set the root's name */
    UString *pName = new UString( getIntf(), p_view->p_root->input.psz_name );
    m_cString = UStringPtr( pName );

    buildNode( p_view->p_root, *this );

    vlc_mutex_unlock( &m_pPlaylist->object_lock );
    checkParents( NULL );
}

