/*****************************************************************************
 * var_tree.cpp
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 *
 * Authors: Antoine Cellerier <dionoea@videolan.org>
 *          Clément Stenac <zorglub@videolan.org>
 *          Erwan   Tulou  <erwan10@videolan.org>
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

#include "var_tree.hpp"
#include <math.h>

const std::string VarTree::m_type = "tree";

VarTree::VarTree( intf_thread_t *pIntf )
    : Variable( pIntf ), m_pParent( NULL ), m_media( NULL ),
      m_readonly( false ), m_selected( false ),
      m_playing( false ), m_expanded( false ),
      m_flat( false ), m_dontMove( false )
{
    // Create the position variable
    m_cPosition = VariablePtr( new VarPercent( pIntf ) );
    getPositionVar().set( 1.0 );

    getPositionVar().addObserver( this );
}

VarTree::VarTree( intf_thread_t *pIntf, VarTree *pParent, input_item_t *media,
                  const UStringPtr &rcString, bool selected, bool playing,
                  bool expanded, bool readonly )
    : Variable( pIntf ), m_pParent( pParent ), m_media( media ),
      m_cString( rcString ),
      m_readonly( readonly ), m_selected( selected ),
      m_playing( playing ), m_expanded( expanded ),
      m_flat( false ), m_dontMove( false )
{
    if( m_media )
        input_item_Hold( m_media );

    // Create the position variable
    m_cPosition = VariablePtr( new VarPercent( pIntf ) );
    getPositionVar().set( 1.0 );

    getPositionVar().addObserver( this );
}

VarTree::VarTree( const VarTree& v )
    : Variable( v.getIntf() ),
      m_children( v.m_children), m_pParent( v.m_pParent ),
      m_media( v.m_media ), m_cString( v.m_cString ),
      m_readonly( v.m_readonly ), m_selected( v.m_selected ),
      m_playing( v.m_playing ), m_expanded( v.m_expanded ),
      m_flat( false ), m_dontMove( false )
{
    if( m_media )
        input_item_Hold( m_media );

    // Create the position variable
    m_cPosition = VariablePtr( new VarPercent( getIntf() ) );
    getPositionVar().set( 1.0 );

    getPositionVar().addObserver( this );
}

VarTree::~VarTree()
{
    getPositionVar().delObserver( this );
    if( m_media )
        input_item_Release( m_media );
}

void VarTree::setMedia( input_item_t *media )
{
    if( m_media )
        input_item_Release( m_media );
    m_media = media;
    if( m_media )
        input_item_Hold( m_media );
}

VarTree::Iterator VarTree::add( input_item_t* media, const UStringPtr &rcString,
                  bool selected, bool playing, bool expanded, bool readonly,
                  int pos )
{
    Iterator it;
    if( pos == -1 )
    {
        it = m_children.end();
    }
    else
    {
        it = m_children.begin();
        for( int i = 0; i < pos && it != m_children.end(); ++it, i++ );
    }

    return m_children.insert( it,
                              VarTree( getIntf(), this, media, rcString,
                                       selected, playing,
                                       expanded, readonly ) );
}

void VarTree::delSelected()
{
    for( Iterator it = m_children.begin(); it != m_children.end(); )
    {
        if( it->m_selected )
        {
            Iterator oldIt = it;
            ++it;
            m_children.erase( oldIt );
        }
    }
}

void VarTree::clear()
{
    m_children.clear();
}

VarTree::Iterator VarTree::getNextSiblingOrUncle()
{
    VarTree *p_parent = parent();
    if( p_parent )
    {
        Iterator it = ++(getSelf());
        if( it != p_parent->m_children.end() )
            return it;
        else
            return next_uncle();
    }
    return root()->m_children.end();
}

VarTree::Iterator VarTree::getPrevSiblingOrUncle()
{
    VarTree *p_parent = parent();
    if( p_parent )
    {
        Iterator it = getSelf();
        if( it != p_parent->m_children.begin() )
            return --it;
        else
            return prev_uncle();
    }
    return root()->m_children.end();
}

/* find iterator to next ancestor
 * ... which means parent++ or grandparent++ or grandgrandparent++ ... */
VarTree::Iterator VarTree::next_uncle()
{
    VarTree *p_parent = parent();
    if( p_parent )
    {
        VarTree *p_grandparent = p_parent->parent();
        while( p_grandparent )
        {
            Iterator it = ++(p_parent->getSelf());
            if( it != p_grandparent->m_children.end() )
                return it;
            p_parent = p_grandparent;
            p_grandparent = p_parent->parent();
        }
    }

    /* if we didn't return before, it means that we've reached the end */
    return root()->m_children.end();
}

VarTree::Iterator VarTree::prev_uncle()
{
    VarTree *p_parent = parent();
    if( p_parent )
    {
        VarTree *p_grandparent = p_parent->parent();
        while( p_grandparent )
        {
            Iterator it = p_parent->getSelf();
            if( it != p_grandparent->m_children.begin() )
                return --it;
            p_parent = p_grandparent;
            p_grandparent = p_parent->parent();
        }
    }

    /* if we didn't return before, it means that we've reached the end */
    return root()->m_children.end();
}

int VarTree::visibleItems()
{
    int i_count = size();
    for( Iterator it = m_children.begin(); it != m_children.end(); ++it )
    {
        if( it->m_expanded )
        {
            i_count += it->visibleItems();
        }
    }
    return i_count;
}

VarTree::Iterator VarTree::getVisibleItem( int n )
{
    Iterator it = m_children.begin();
    while( it != m_children.end() )
    {
        n--;
        if( n <= 0 )
            return it;
        if( it->m_expanded )
        {
            int i;
            i = n - it->visibleItems();
            if( i <= 0 ) return it->getVisibleItem( n );
            n = i;
        }
        ++it;
    }
    return m_children.end();
}

VarTree::Iterator VarTree::getLeaf( int n )
{
    Iterator it = m_children.begin();
    while( it != m_children.end() )
    {
        if( it->size() )
        {
            int i;
            i = n - it->countLeafs();
            if( i <= 0 ) return it->getLeaf( n );
            n = i;
        }
        else
        {
            n--;
            if( n <= 0 )
                return it;
        }
        ++it;
    }
    return m_children.end();
}

VarTree::Iterator VarTree::getNextVisibleItem( Iterator it )
{
    if( it->m_expanded && it->size() )
    {
        it = it->m_children.begin();
    }
    else
    {
        Iterator it_old = it;
        ++it;
        // Was 'it' the last brother? If so, look for uncles
        if( it_old->parent() && it_old->parent()->m_children.end() == it )
        {
            it = it_old->next_uncle();
        }
    }
    return it;
}

VarTree::Iterator VarTree::getPrevVisibleItem( Iterator it )
{
    if( it == root()->m_children.begin() )
        return it;

    if( it == root()->m_children.end() )
    {
        --it;
        while( it->size() && it->m_expanded )
            it = --(it->m_children.end());
        return it;
    }

    /* Was it the first child of its parent ? */
    VarTree *p_parent = it->parent();
    if( it == p_parent->m_children.begin() )
    {
        /* Yes, get its parent's it */
        it = p_parent->getSelf();
    }
    else
    {
        --it;
        /* We have found an older brother, take its last visible child */
        while( it->size() && it->m_expanded )
            it = --(it->m_children.end());
    }
    return it;
}

VarTree::Iterator VarTree::getNextItem( Iterator it )
{
    if( it->size() )
    {
        it = it->m_children.begin();
    }
    else
    {
        Iterator it_old = it;
        ++it;
        // Was 'it' the last brother? If so, look for uncles
        if( it_old->parent() && it_old->parent()->m_children.end() == it )
        {
            it = it_old->next_uncle();
        }
    }
    return it;
}

VarTree::Iterator VarTree::getPrevItem( Iterator it )
{
    if( it == root()->m_children.begin() )
        return it;

    if( it == root()->m_children.end() )
    {
        --it;
        while( it->size() )
            it = --(it->m_children.end());
        return it;
    }
    /* Was it the first child of its parent ? */
    VarTree *p_parent = it->parent();
    if( it == p_parent->m_children.begin() )
    {
        /* Yes, get its parent's it */
        it = p_parent->getSelf();
    }
    else
    {
        --it;
        /* We have found an older brother, take its last child */
        while( it->size() )
            it = --(it->m_children.end());
    }
    return it;
}

VarTree::Iterator VarTree::getNextLeaf( Iterator it )
{
    do
    {
        it = getNextItem( it );
    }
    while( it != root()->m_children.end() && it->size() );
    return it;
}

VarTree::Iterator VarTree::getPrevLeaf( Iterator it )
{
    Iterator it_new = it->getPrevSiblingOrUncle();
    if( it_new == root()->end() )
        return it_new;
    while( it_new->size() )
        it_new = --(it_new->m_children.end());
    return it_new;
}

VarTree::Iterator VarTree::getParent( Iterator it )
{
    if( it->parent() )
    {
        return it->parent()->getSelf();
    }
    return m_children.end();
}

void VarTree::ensureExpanded( const Iterator& it )
{
    /// Don't expand ourselves, only our parents
    VarTree *current = &(*it);
    current = current->parent();
    while( current->parent() )
    {
        current->m_expanded = true;
        current = current->parent();
    }
}

int VarTree::countLeafs()
{
    if( size() == 0 )
        return 1;

    int i_count = 0;
    for( Iterator it = m_children.begin(); it != m_children.end(); ++it )
    {
        i_count += it->countLeafs();
    }
    return i_count;
}

VarTree::Iterator VarTree::firstLeaf()
{
    Iterator b = root()->m_children.begin();
    if( b->size() ) return getNextLeaf( b );
    return b;
}

int VarTree::getIndex( const Iterator& item )
{
    int index = 0;
    Iterator it;
    for( it = m_flat ? firstLeaf() : m_children.begin();
         it != m_children.end();
         it = m_flat ? getNextLeaf( it ) : getNextVisibleItem( it ) )
    {
        if( it == item )
            break;
        index++;
    }
    return (it == item) ? index : -1;
}

VarTree::Iterator VarTree::getItemFromSlider()
{
    // a simple (int)(...) causes rounding errors !
#ifdef _MSC_VER
#       define lrint (int)
#endif
    VarPercent &rVarPos = getPositionVar();
    double percentage = rVarPos.get();

    int indexMax = m_flat ? (countLeafs() - 1)
                          : (visibleItems() - 1);

    int index = lrint( (1.0 - percentage)*(double)indexMax );

    Iterator it_first = m_flat ? getLeaf( index + 1 )
                               : getVisibleItem( index + 1 );
    return it_first;
}

void VarTree::setSliderFromItem( const Iterator& it )
{
    VarPercent &rVarPos = getPositionVar();

    int indexMax = m_flat ? (countLeafs() - 1)
                          : (visibleItems() - 1);

    int index = getIndex( it );
    double percentage = (1.0 - (double)index/(double)indexMax);

    m_dontMove = true;
    rVarPos.set( (float)percentage );
    m_dontMove = false;
}

void VarTree::onUpdate( Subject<VarPercent> &rPercent, void* arg )
{
    (void)rPercent; (void)arg;
    onUpdateSlider();
}

void VarTree::unselectTree()
{
    m_selected = false;
    for( Iterator it = m_children.begin(); it != m_children.end(); ++it )
        it->unselectTree();
}

VarTree::IteratorVisible VarTree::getItem( int index )
{
   Iterator it =
        m_flat ? getLeaf( index + 1 )
               : getVisibleItem( index + 1 );

   return IteratorVisible( it, this );
}
