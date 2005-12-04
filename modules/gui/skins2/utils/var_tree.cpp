/*****************************************************************************
 * var_tree.cpp
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

#include "var_tree.hpp"


const string VarTree::m_type = "tree";

VarTree::VarTree( intf_thread_t *pIntf )
    : Variable( pIntf ), m_id( 0 ), m_selected( false ), m_playing( false ),
    m_expanded( false ), m_pData( NULL ), m_pParent( NULL )
{
    // Create the position variable
    m_cPosition = VariablePtr( new VarPercent( pIntf ) );
    getPositionVar().set( 1.0 );
}

VarTree::VarTree( intf_thread_t *pIntf, VarTree *pParent, int id,
                  const UStringPtr &rcString, bool selected, bool playing,
                  bool expanded, void *pData )
    : Variable( pIntf ), m_id( id ), m_cString( rcString ),
    m_selected( selected ), m_playing( playing ), m_expanded( expanded ),
    m_pData( pData ), m_pParent( pParent )
{
    // Create the position variable
    m_cPosition = VariablePtr( new VarPercent( pIntf ) );
    getPositionVar().set( 1.0 );
}

VarTree::~VarTree()
{
// TODO : check that children are deleted
}

void VarTree::add( int id, const UStringPtr &rcString, bool selected,
                   bool playing, bool expanded, void *pData )
{
    m_children.push_back( VarTree( getIntf(), this, id, rcString, selected,
                                   playing, expanded, pData ) );
}

void VarTree::delSelected()
{
    Iterator it = begin();
    while( it != end() )
    {
        //dig down the tree
        if( size() ) it->delSelected();
        //stay on some level
        if( it->m_selected )
        {
            Iterator oldIt = it;
            it++;
            m_children.erase( oldIt );
        }
        else
        {
            it++;
        }
    }
}

void VarTree::clear()
{
    m_children.clear();
}

VarTree::Iterator VarTree::operator[]( int n )
{
    Iterator it;
    int i;
    for( it = begin(), i = 0;
         i < n && it != end();
         it++, i++ );
    return it;
}

VarTree::ConstIterator VarTree::operator[]( int n ) const
{
    ConstIterator it;
    int i;
    for( it = begin(), i = 0;
         i < n && it != end();
         it++, i++ );
    return it;
}

/* find iterator to next ancestor
 * ... which means parent++ or grandparent++ or grandgrandparent++ ... */
VarTree::Iterator VarTree::uncle()
{
    VarTree *p_parent = parent();
    if( p_parent != NULL )
    {
        VarTree *p_grandparent = p_parent->parent();
        while( p_grandparent != NULL )
        {
            Iterator it = p_grandparent->begin();
            while( it != p_grandparent->end() && &(*it) != p_parent ) it++;
            if( it != p_grandparent->end() )
            {
                it++;
                if( it != p_grandparent->end() )
                {
                    return it;
                }
            }
            if( p_grandparent->parent() )
            {
                p_parent = p_grandparent;
                p_grandparent = p_parent->parent();
            }
            else
                p_grandparent = NULL;
        }
    }

    /* if we didn't return before, it means that we've reached the end */
    return root()->end();
}

void VarTree::checkParents( VarTree *pParent )
{
    m_pParent = pParent;
    Iterator it = begin();
    while( it != end() )
    {
        it->checkParents( this );
        it++;
    }
}

int VarTree::visibleItems()
{
    int i_count = size();
    Iterator it = begin();
    while( it != end() )
    {
        if( it->m_expanded )
        {
            i_count += it->visibleItems();
        }
        it++;
    }
    return i_count;
}

VarTree::Iterator VarTree::getVisibleItem( int n )
{
    Iterator it = begin();
    while( it != end() )
    {
        n--;
        if( n <= 0 ) return it;
        if( it->m_expanded )
        {
            int i = n - it->visibleItems();
            if( i <= 0 ) return it->getVisibleItem( n );
            n = i;
        }
        it++;
    }
    return end();
}

VarTree::Iterator VarTree::getNextVisibleItem( Iterator it )
{
    if( it->m_expanded && it->size() )
    {
        it = it->begin();
    }
    else
    {
        VarTree::Iterator it_old = it;
        it++;
        // Was 'it' the last brother? If so, look for uncles
        if( it_old->parent() && it_old->parent()->end() == it )
        {
            it = it_old->uncle();
        }
    }
    return it;
}

VarTree::Iterator VarTree::getNextItem( Iterator it )
{
    if( it->size() )
    {
        it = it->begin();
    }
    else
    {
        VarTree::Iterator it_old = it;
        it++;
        // Was 'it' the last brother? If so, look for uncles
        if( it_old->parent() && it_old->parent()->end() == it )
        {
            it = it_old->uncle();
        }
    }
    return it;
}


VarTree::Iterator VarTree::findById( int id )
{
    for (Iterator it = begin(); it != end(); ++it )
    {
        if( it->m_id == id )
        {
            return it;
        }
        Iterator result = it->findById( id );
        if( result != it->end() ) return result;
    }
    return end();
}


void VarTree::ensureExpanded( VarTree::Iterator it )
{
    /// Don't expand ourselves, only our parents
    VarTree *current = &(*it);
    current = current->parent();
    while( current->parent() != NULL )
    {
        current->m_expanded = true;
        current = current->parent();
    }
}
