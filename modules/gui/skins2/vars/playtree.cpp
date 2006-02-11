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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include <vlc/vlc.h>

#include "playtree.hpp"
#include "../utils/ustring.hpp"

#include "charset.h"


Playtree::Playtree( intf_thread_t *pIntf ): VarTree( pIntf )
{
    // Get the VLC playlist object
    m_pPlaylist = pIntf->p_sys->p_playlist;

    i_items_to_append = 0;

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
}

void Playtree::delSelected()
{
    Iterator it;
    for (it = begin(); it != end() ; it = getNextVisibleItem( it ) )
    {
        if( (*it).m_selected )
        {
            playlist_item_t *p_item = (playlist_item_t *)(it->m_pData);
            if( p_item->i_children == -1 )
            {
                playlist_LockDelete( getIntf()->p_sys->p_playlist,
                                     p_item->input.i_id );
            }
            else
            {
                vlc_mutex_lock( &getIntf()->p_sys->p_playlist->object_lock );
                playlist_NodeDelete( getIntf()->p_sys->p_playlist, p_item,
                                     VLC_TRUE, VLC_FALSE );
                vlc_mutex_unlock( &getIntf()->p_sys->p_playlist->object_lock );
            }
        }
    }
    /// \todo Do this better (handle item-deleted)
    buildTree();
    tree_update descr;
    descr.i_type = 1;
    notify( &descr );
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
    tree_update descr;
    descr.i_type = 1;
    notify( &descr );
}

void Playtree::onUpdateItem( int id )
{
    Iterator it = findById( id );
    tree_update descr;
    descr.b_active_item = false;
    if( it != end() )
    {
        // Update the item
        playlist_item_t* pNode = (playlist_item_t*)(it->m_pData);
        UString *pName = new UString( getIntf(), pNode->input.psz_name );
        it->m_cString = UStringPtr( pName );
        it->m_playing = m_pPlaylist->status.p_item == pNode;
        if( it->m_playing ) descr.b_active_item = true;
    }
    else
    {
        msg_Warn(getIntf(), "Cannot find node with id %d", id );
    }
    descr.i_type = 0;
    notify( &descr );
}

void Playtree::onAppend( playlist_add_t *p_add )
{
    i_items_to_append --;

    Iterator node = findById( p_add->i_node );
    if( node != end() )
    {
        Iterator item =  findById( p_add->i_item );
        if( item == end() )
        {
            playlist_item_t *p_item = playlist_ItemGetById(
                                        m_pPlaylist, p_add->i_item );
            if( !p_item ) return;
            UString *pName = new UString( getIntf(), p_item->input.psz_name );
            node->add( p_add->i_item, UStringPtr( pName ),
                      false,false, false, p_item );
        }
    }
    tree_update descr;
    descr.i_id = p_add->i_item;
    descr.i_parent = p_add->i_node;
    descr.b_visible = node->m_expanded;
    descr.i_type = 2;
    notify( &descr );
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
                     false, pNode->pp_children[i] );
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

    i_items_to_append = 0;

    playlist_view_t *p_view;
    p_view = playlist_ViewFind( m_pPlaylist, VIEW_CATEGORY );
    /// \todo let the user chose the view

    clear();

    /* Set the root's name */
    UString *pName = new UString( getIntf(), p_view->p_root->input.psz_name );
    m_cString = UStringPtr( pName );

    buildNode( p_view->p_root, *this );

    vlc_mutex_unlock( &m_pPlaylist->object_lock );
//  What is it ?
//    checkParents( NULL );
}

