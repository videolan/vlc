/*****************************************************************************
 * playtree.cpp
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 *
 * Authors: Antoine Cellerier <dionoea@videolan.org>
 *          Clément Stenac <zorglub@videolan.org>
 *          Erwan Tulou    <erwan10@videolan.org>
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
#include <vlc_input_item.h>
#include <vlc_url.h>
#include "../utils/ustring.hpp"

Playtree::Playtree( intf_thread_t *pIntf )
    : VarTree( pIntf ), m_currentIndex( -1 )
{
    getPositionVar().addObserver( this );
    buildTree();
}

Playtree::~Playtree()
{
    getPositionVar().delObserver( this );
}

void Playtree::delSelected()
{
    for( Iterator it = m_children.begin(); it != m_children.end(); )
    {
        if( it->isSelected() && !it->isReadonly() )
        {
            vlc_playlist_Lock( getPL() );
            ssize_t idx = vlc_playlist_IndexOfMedia( getPL(), it->getMedia() );
            if( idx != -1 )
                vlc_playlist_Remove( getPL(), idx, 1 ),
            vlc_playlist_Unlock( getPL() );

            it = it->getNextSiblingOrUncle();
        }
        else
        {
            it = getNextItem( it );
        }
    }
}

void Playtree::action( VarTree *pElem )
{
    if( !pElem->getMedia() )
        return;
    vlc_playlist_Lock( getPL() );
    ssize_t idx = vlc_playlist_IndexOfMedia( getPL(), pElem->getMedia() );
    if( idx != -1 )
        vlc_playlist_PlayAt( getPL(), idx );
    vlc_playlist_Unlock( getPL() );
}

void Playtree::onChange()
{
    buildTree();
    tree_update descr( tree_update::ResetAll, end() );
    notify( &descr );
}

void Playtree::onUpdateItem( int pos )
{
    Iterator it = getPlaylistIt( pos  );
    if( it != m_children.end() )
    {
        vlc_playlist_Lock( getPL() );
        vlc_playlist_item_t *item = vlc_playlist_Get( getPL(), pos );
        input_item_t* media = vlc_playlist_item_GetMedia( item ) ;
        if( media != it->getMedia() )
            it->setMedia( media );
        vlc_playlist_Unlock( getPL() );

        bool updateNeeded = false;

        bool isPlaying = (pos == m_currentIndex);
        if( it->isPlaying() != isPlaying )
        {
            it->setPlaying( isPlaying );
            updateNeeded = true;
        }

        UString *pName = getTitle( media );
        if( *pName != *(it->getString()) )
        {
            it->setString( UStringPtr( pName ) );
            updateNeeded = true;
        }
        else
        {
            delete pName;
        }

        if( updateNeeded )
        {
            tree_update descr(
                tree_update::ItemUpdated, IteratorVisible( it, this ) );
            notify( &descr );
        }
    }
}

void Playtree::onUpdatePlaying( int pos )
{
    if( m_currentIndex != -1 && m_currentIndex != pos )
    {
        // de-highlight previous item
        Iterator it = getPlaylistIt( m_currentIndex );
        it->setPlaying( false );

        tree_update descr(
            tree_update::ItemUpdated, IteratorVisible( it, this ) );
        notify( &descr );
    }
    m_currentIndex = pos;
    if( m_currentIndex != -1 )
    {
        // highlight new item
        Iterator it = getPlaylistIt( m_currentIndex );
        it->setPlaying( true );

        tree_update descr(
            tree_update::ItemUpdated, IteratorVisible( it, this ) );
        notify( &descr );
    }
}

void Playtree::onDelete( int pos )
{
    Iterator it = getPlaylistIt( pos );
    if( it != m_children.end() )
    {
        VarTree* parent = it->parent();
        if( parent )
        {
            tree_update descr(
                tree_update::DeletingItem, IteratorVisible( it, this ) );
            notify( &descr );

            parent->removeChild( it );

            tree_update descr2(
                tree_update::ItemDeleted, end() );
            notify( &descr2 );
        }
    }
}

void Playtree::onAppend( int pos )
{
    Iterator it_node = m_children.begin();
    it_node->setExpanded( true );

    vlc_playlist_Lock( getPL() );
    vlc_playlist_item_t *item = vlc_playlist_Get( getPL(), pos );
    input_item_t* media = vlc_playlist_item_GetMedia( item ) ;
    bool isPlaying = pos == vlc_playlist_GetCurrentIndex( getPL() );
    vlc_playlist_Unlock( getPL() );

    UString *pName = getTitle( media );
    Iterator it = it_node->add( media, UStringPtr( pName ),
                                false, isPlaying, false, false, pos );

    tree_update descr( tree_update::ItemInserted,
                       IteratorVisible( it, this ) );
    notify( &descr );
}

void Playtree::buildNode( int pos, VarTree &rTree )
{
    vlc_playlist_item_t *item = vlc_playlist_Get( getPL(), pos );
    input_item_t *media = vlc_playlist_item_GetMedia( item );
    UString *pName = getTitle( media );
    bool isPlaying = pos == vlc_playlist_GetCurrentIndex(getPL());
    (void)rTree.add( media, UStringPtr( pName ),
                     false, isPlaying, false, false, -1 );
}

void Playtree::buildTree()
{
    clear();

    // build playlist entry
    UString *pName = new UString( getIntf(), _( "Playlist" ) );
    Iterator it = add( NULL, UStringPtr( pName ),
                       false, false, false, false, -1 );

    vlc_playlist_Lock( getPL() );
    size_t count = vlc_playlist_Count( getPL() );
    for( size_t pos = 0; pos < count; pos++ )
    {
        if( pos == 0 )
            it->setExpanded( true );
        buildNode( pos , *it );
    }
    vlc_playlist_Unlock( getPL() );
}

void Playtree::onUpdateSlider()
{
    tree_update descr( tree_update::SliderChanged, end() );
    notify( &descr );
}

void Playtree::insertItems( VarTree& elem, const std::list<std::string>& files, bool start )
{
    bool first = start;
    VarTree* p_elem = &elem;
    VarTree* pNode = NULL;
    int i_pos = -1;

    if( p_elem == this )
        p_elem = &*getPlaylistIt();

    if( p_elem == &*getPlaylistIt() )
    {
        pNode = p_elem;
        i_pos = 0;
        p_elem->setExpanded( true );
    }
    else
    if( p_elem->size() && p_elem->isExpanded() )
    {
        pNode = p_elem;
        i_pos = 0;
    }
    else
    {
        pNode = p_elem->parent() ? p_elem->parent() : p_elem;
        i_pos = p_elem->getIndex();
        i_pos++;
    }

    assert( pNode != NULL );

    vlc_playlist_Lock( getPL() );
    for( std::list<std::string>::const_iterator it = files.begin();
         it != files.end(); ++it, i_pos++ )
    {
        input_item_t *pItem;

        if( strstr( it->c_str(), "://" ) )
            pItem = input_item_New( it->c_str(), NULL );
        else
        {
            char *psz_uri = vlc_path2uri( it->c_str(), NULL );
            if( psz_uri == NULL )
                continue;
            pItem = input_item_New( psz_uri, NULL );
            free( psz_uri );
        }

        if( pItem == NULL)
            continue;

        vlc_playlist_Insert( getPL(),i_pos, &pItem, 1 );
        if( first )
        {
            first = false;
            vlc_playlist_PlayAt( getPL(),i_pos );
        }
    }
    vlc_playlist_Unlock( getPL() );

}

UString* Playtree::getTitle( input_item_t *pItem )
{
    char *psz_name = input_item_GetTitleFbName( pItem );
    UString *pTitle = new UString( getIntf(), psz_name );
    free( psz_name );
    return pTitle;
}

VarTree::Iterator Playtree::getPlaylistIt( int pos )
{
    // Playlist Node
    Iterator it = m_children.begin();
    if( pos == -1 )
        return it;

    // first playlist item  (pos=0)
    it = getNextItem( it );
    for( int i = 0; i < pos; i++ )
        it = getNextItem( it );
    return it;
}
