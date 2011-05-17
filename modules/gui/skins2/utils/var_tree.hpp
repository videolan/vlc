/*****************************************************************************
 * var_tree.hpp
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VAR_TREE_HPP
#define VAR_TREE_HPP

#include <list>
#include <assert.h>

#include "variable.hpp"
#include "observer.hpp"
#include "ustring.hpp"
#include "var_percent.hpp"

class VarTree;
struct tree_update;

/// Tree variable
class VarTree: public Variable,
               public Subject<VarTree, tree_update>,
               public Observer<VarPercent>
{
public:
    VarTree( intf_thread_t *pIntf );

    VarTree( intf_thread_t *pIntf, VarTree *pParent, int id,
             const UStringPtr &rcString, bool selected, bool playing,
             bool expanded, bool readonly );
    VarTree( const VarTree& );

    virtual ~VarTree();

    /// Iterators
    typedef list<VarTree>::iterator Iterator;
    typedef list<VarTree>::const_iterator ConstIterator;

    /// Get the variable type
    virtual const string &getType() const { return m_type; }

    /// Add a pointer on string in the children's list
    virtual Iterator add( int id, const UStringPtr &rcString, bool selected,
                    bool playing, bool expanded, bool readonly, int pos = -1 );

    /// Remove the selected item from the children's list
    virtual void delSelected();

    /// Remove all elements from the children's list
    virtual void clear();

    inline int  getId() { return m_id; }
    inline UString* getString() {return (UString*)m_cString.get(); }
    inline void setString( UStringPtr val ) { m_cString = val; }

    inline bool isReadonly() { return m_readonly; };
    inline bool isSelected() { return m_selected; };
    inline bool isPlaying() { return m_playing; };
    inline bool isExpanded() { return m_expanded; };
    inline bool isFlat() { return m_flat; };

    inline void setSelected( bool val ) { m_selected = val; }
    inline void setPlaying( bool val ) { m_playing = val; }
    inline void setExpanded( bool val ) { m_expanded = val; }
    inline void setFlat( bool val ) { m_flat = val; }

    inline void toggleSelected() { m_selected = !m_selected; }
    inline void toggleExpanded() { setExpanded( !m_expanded ); }

    /// Get the number of children
    int size() const { return m_children.size(); }

    /// iterator over visible items
    class IteratorVisible : public Iterator
    {
        public:
        IteratorVisible( const VarTree::Iterator& it, VarTree* pRootTree )
            : VarTree::Iterator( it ), m_pRootTree( pRootTree ) {}

        IteratorVisible& operator++()
        {
            Iterator& it = *this;
            assert( it != end() );
            it = isFlat() ? m_pRootTree->getNextLeaf( it ) :
                            m_pRootTree->getNextVisibleItem( it );
            return *this;
        }

        IteratorVisible& operator--()
        {
            Iterator& it = *this;
            it = isFlat() ? m_pRootTree->getPrevLeaf( it ) :
                            m_pRootTree->getPrevVisibleItem( it );
            return *this;
        }

        IteratorVisible getParent()
        {
            IteratorVisible& it = *this;
            if( it->parent() && it->parent() != m_pRootTree )
            {
                return IteratorVisible( it->parent()->getSelf(), m_pRootTree );
            }
            return end();
        }

        private:
        inline IteratorVisible begin() { return m_pRootTree->begin(); }
        inline IteratorVisible end()   { return m_pRootTree->end(); }
        inline bool isFlat()           { return m_pRootTree->m_flat; }
        VarTree* m_pRootTree;
    };

    /// Beginning of the children's list
    IteratorVisible begin()
    {
        return IteratorVisible(
               m_flat ? firstLeaf() : m_children.begin(), this );
    }

    /// End of children's list
    IteratorVisible end() { return IteratorVisible( m_children.end(), this ); }

    /// Back of children's list
    VarTree &back() { return m_children.back(); }

    /// Parent node
    VarTree *parent() { return m_pParent; }

    /// Get next sibling
    Iterator getNextSiblingOrUncle();
    Iterator getPrevSiblingOrUncle();

    Iterator getSelf()
    {
        assert( m_pParent );
        Iterator it = m_pParent->m_children.begin();
        for( ; &*it != this && it != m_pParent->m_children.end(); ++it );
        assert( it != m_pParent->m_children.end() );
        return it;
    }

    int getIndex()
    {
        if( m_pParent )
        {
            int i_pos = 0;
            for( Iterator it = m_pParent->m_children.begin();
                 it != m_pParent->m_children.end(); ++it, i_pos++ )
                if( &(*it) == this )
                    return i_pos;
        }
        return -1;
    }

    Iterator next_uncle();
    Iterator prev_uncle();

    /// Get first leaf
    Iterator firstLeaf();

    /// Remove a child
    void removeChild( Iterator it ) { m_children.erase( it ); }

    /// Execute the action associated to this item
    virtual void action( VarTree *pItem ) { VLC_UNUSED(pItem); }

    /// Get a reference on the position variable
    VarPercent &getPositionVar() const
    { return *((VarPercent*)m_cPosition.get()); }

    /// Get a counted pointer on the position variable
    const VariablePtr &getPositionVarPtr() const { return m_cPosition; }

    /// Count the number of items that should be displayed if the
    /// playlist window wasn't limited
    int visibleItems();

    /// Count the number of leafs in the tree
    int countLeafs();

    /// Return iterator to the n'th visible item
    Iterator getVisibleItem( int n );

    /// Return iterator to the n'th leaf
    Iterator getLeaf( int n );

    /// Given an iterator to a visible item, return the next visible item
    Iterator getNextVisibleItem( Iterator it );

    /// Given an it to a visible item, return the previous visible item
    Iterator getPrevVisibleItem( Iterator it );

    /// Given an iterator to an item, return the next item
    Iterator getNextItem( Iterator it );

    /// Given an iterator to an item, return the previous item
    Iterator getPrevItem( Iterator it );

    /// Given an iterator to an item, return the next leaf
    Iterator getNextLeaf( Iterator it );

    /// Given an iterator to an item, return the previous leaf
    Iterator getPrevLeaf( Iterator it );

    /// Given an iterator to an item, return the parent item
    Iterator getParent( Iterator it );

    /// return index of visible item (starting from 0)
    int getIndex( const Iterator& it );

    /// Ensure an item is expanded
    void ensureExpanded( const Iterator& it );

    ///
    Iterator getItemFromSlider();
    void setSliderFromItem( const Iterator& it );

    ///
    void onUpdate( Subject<VarPercent> &rPercent, void* arg);

    /// Get depth (root depth is 0)
    int depth()
    {
        VarTree *parent = this;
        int depth = 0;
        while( ( parent = parent->parent() ) != NULL )
            depth++;
        return depth;
    }

    virtual void onUpdateSlider() {}

    void unselectTree();

    VarTree::IteratorVisible getItem( int index );

protected:

    /// List of children
    list<VarTree> m_children;

private:

    /// Get root node
    VarTree *root()
    {
        VarTree *parent = this;
        while( parent->parent() != NULL )
            parent = parent->parent();
        return parent;
    }

    /// Pointer to parent node
    VarTree *m_pParent;

    int m_id;
    UStringPtr m_cString;

    /// indicators
    bool m_readonly;
    bool m_selected;
    bool m_playing;
    bool m_expanded;
    bool m_flat;
    bool m_dontMove;

    /// Variable type
    static const string m_type;

    /// Position variable
    VariablePtr m_cPosition;
};

/// Description of an update to the tree
typedef struct tree_update
{
    enum type_t
    {
        ItemUpdated,
        ItemInserted,
        ItemDeleted,
        DeletingItem,
        ResetAll,
        SliderChanged,
    };
    enum type_t type;
    VarTree::IteratorVisible it;

    tree_update( enum type_t t, VarTree::IteratorVisible item ) :
        type( t ), it( item ) {}
} tree_update;

#endif
