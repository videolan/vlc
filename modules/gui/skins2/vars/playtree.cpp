/*****************************************************************************
 * playtree.cpp
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea@videolan.org>
 *          Clément Stenac <zorglub@videolan.org>
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

#include "playtree.hpp"
#include <vlc_playlist.h>
#include "../utils/ustring.hpp"

#include "vlc_charset.h"


Playtree::Playtree( intf_thread_t *pIntf ): VarTree( pIntf )
{
    // Get the VLC playlist object
    m_pPlaylist = pIntf->p_sys->p_playlist;

    i_items_to_append = 0;

    buildTree();
}

Playtree::~Playtree()
{
}

void Playtree::delSelected()
{
    Iterator it = begin();
    playlist_Lock( getIntf()->p_sys->p_playlist );
    for( it = begin(); it != end(); it = getNextVisibleItem( it ) )
    {
        if( (*it).m_selected && !(*it).isReadonly() )
        {
            (*it).m_deleted = true;
        }
    }
    /// \todo Do this better (handle item-deleted)
    tree_update descr;
    descr.i_type = 3;
    notify( &descr );
    it = begin();
    while( it != end() )
    {
        if( (*it).m_deleted )
        {
            VarTree::Iterator it2;
            playlist_item_t *p_item = (playlist_item_t *)(it->m_pData);
            if( p_item->i_children == -1 )
            {
                playlist_DeleteFromInput( getIntf()->p_sys->p_playlist,
                                          p_item->p_input->i_id, pl_Locked );
                it2 = getNextVisibleItem( it ) ;
                it->parent()->removeChild( it );
                it = it2;
            }
            else
            {
                playlist_NodeDelete( getIntf()->p_sys->p_playlist, p_item,
                                     true, false );
                it2 = getNextSibling( it );
                it->parent()->removeChild( it );
                it = it2;
            }
        }
        else
        {
            it = getNextVisibleItem( it );
        }
    }
    playlist_Unlock( getIntf()->p_sys->p_playlist );
}

void Playtree::action( VarTree *pItem )
{
    playlist_Lock( m_pPlaylist );
    VarTree::Iterator it;

    playlist_item_t *p_item = (playlist_item_t *)pItem->m_pData;
    playlist_item_t *p_parent = p_item;
    while( p_parent )
    {
        if( p_parent == m_pPlaylist->p_root_category )
            break;
        p_parent = p_parent->p_parent;
    }

    if( p_parent )
    {
        playlist_Control( m_pPlaylist, PLAYLIST_VIEWPLAY, pl_Locked, p_parent, p_item );
    }
    playlist_Unlock( m_pPlaylist );
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
        UString *pName = new UString( getIntf(), pNode->p_input->psz_name );
        it->m_cString = UStringPtr( pName );
        playlist_Lock( m_pPlaylist );
        it->m_playing = playlist_CurrentPlayingItem( m_pPlaylist ) == pNode;
        playlist_Unlock( m_pPlaylist );
        if( it->m_playing ) descr.b_active_item = true;
    }
    else
    {
        msg_Warn(getIntf(), "cannot find node with id %d", id );
    }
    descr.i_type = 0;
    notify( &descr );
}

/// \todo keep a list of "recently removed" to avoid looking up if we
//  already removed it
void Playtree::onDelete( int i_id )
{
    tree_update descr;
    descr.i_id = i_id;
    descr.i_type = 3;
    Iterator item = findById( i_id ) ;
    if( item != end() )
    {
        if( item->parent() )
            item->parent()->removeChild( item );
        descr.b_visible = item->parent() ? true : item->parent()->m_expanded;
        notify( &descr );
    }
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
            playlist_Lock( m_pPlaylist );
            playlist_item_t *p_item = playlist_ItemGetById(
                                        m_pPlaylist, p_add->i_item );
            if( !p_item )
            {
                playlist_Unlock( m_pPlaylist );
                return;
            }
            UString *pName = new UString( getIntf(),
                                          p_item->p_input->psz_name );
            node->add( p_add->i_item, UStringPtr( pName ),
                      false,false, false, p_item->i_flags & PLAYLIST_RO_FLAG,
                      p_item );
            playlist_Unlock( m_pPlaylist );
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
                                   pNode->pp_children[i]->p_input->psz_name );
        rTree.add( pNode->pp_children[i]->i_id, UStringPtr( pName ),
                     false,
                     playlist_CurrentPlayingItem(m_pPlaylist) == pNode->pp_children[i],
                     false, pNode->pp_children[i]->i_flags & PLAYLIST_RO_FLAG,
                     pNode->pp_children[i] );
        if( pNode->pp_children[i]->i_children )
        {
            buildNode( pNode->pp_children[i], rTree.back() );
        }
    }
}

void Playtree::buildTree()
{
    clear();
    playlist_Lock( m_pPlaylist );

    i_items_to_append = 0;

    clear();

    /* TODO: Let user choose view - Stick with category ATM */

    /* Set the root's name */
    UString *pName = new UString( getIntf(),
                             m_pPlaylist->p_root_category->p_input->psz_name );
    m_cString = UStringPtr( pName );

    buildNode( m_pPlaylist->p_root_category, *this );

    playlist_Unlock( m_pPlaylist );
//  What is it ?
//    checkParents( NULL );
}

