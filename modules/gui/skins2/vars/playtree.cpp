/*****************************************************************************
 * playtree.cpp
 *****************************************************************************
 * Copyright (C) 2005 VideoLAN
 * $Id: playlist.hpp 8659 2004-09-07 21:16:49Z gbazin $
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

Playtree::Playtree( intf_thread_t *pIntf )
         :VarTree( pIntf, /*m_parent = */NULL )
{
    // Get the VLC playlist object
    m_pPlaylist = pIntf->p_sys->p_playlist;

    // Try to guess the current charset
    char *pCharset;
    vlc_current_charset( &pCharset );
    iconvHandle = vlc_iconv_open( "UTF-8", pCharset );
    msg_Dbg( pIntf, "Using character encoding: %s", pCharset );
    free( pCharset );

    if( iconvHandle == (vlc_iconv_t)-1 )
    {
        msg_Warn( pIntf, "Unable to do requested conversion" );
    }

    buildTree();
}

Playtree::~Playtree()
{
    if( iconvHandle != (vlc_iconv_t)-1 ) vlc_iconv_close( iconvHandle );
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
    /* FIXME : updateTree could be a nice idea so we don't have to
     * start from scratch each time the playlist changes */
    buildTree();
    notify();
}

void Playtree::buildNode( playlist_item_t *p_node, VarTree &m_pNode )
{
    fprintf( stderr, "[32;1mPlaytree::buildNode[0m\n");
    for( int i = 0; i < p_node->i_children; i++ )
    {
        fprintf( stderr, "[33;1m"__FILE__ "%d :[0m adding playtree item : %s\n", __LINE__, p_node->pp_children[i]->input.psz_name );
        UString *pName = new UString( getIntf(), p_node->pp_children[i]->input.psz_name );
        m_pNode.add( UStringPtr( pName ),
                     false,
                     m_pPlaylist->status.p_item == p_node->pp_children[i],
                     true,
                     p_node->pp_children[i] );
        if( p_node->pp_children[i]->i_children )
        {
            buildNode( p_node->pp_children[i], m_pNode.back() );
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
