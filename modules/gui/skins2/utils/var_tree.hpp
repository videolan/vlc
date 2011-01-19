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

#include "variable.hpp"
#include "observer.hpp"
#include "ustring.hpp"
#include "var_percent.hpp"

/// Description of an update to the tree
typedef struct tree_update
{
    enum type_t
    {
        UpdateItem,
        AppendItem,
        DeleteItem,
        ResetAll,
    };

    enum type_t type;
    int i_id;
    bool b_active_item;

} tree_update;

/// Tree variable
class VarTree: public Variable, public Subject<VarTree, tree_update>
{
public:
    VarTree( intf_thread_t *pIntf );

    VarTree( intf_thread_t *pIntf, VarTree *pParent, int id,
             const UStringPtr &rcString, bool selected, bool playing,
             bool expanded, bool readonly, void *pData );

    virtual ~VarTree();

    /// Get the variable type
    virtual const string &getType() const { return m_type; }

    /// Add a pointer on string in the children's list
    virtual void add( int id, const UStringPtr &rcString, bool selected,
                      bool playing, bool expanded, bool readonly, void *pData );

    /// Remove the selected item from the children's list
    virtual void delSelected();

    /// Remove all elements from the children's list
    virtual void clear();

    inline int  getId() { return m_id; }
    inline void *getData() { return m_pData; }
    inline UString* getString() {return (UString*)m_cString.get(); }
    inline void setString( UStringPtr val ) { m_cString = val; }

    inline bool isReadonly() { return m_readonly; };
    inline bool isSelected() { return m_selected; };
    inline bool isPlaying() { return m_playing; };
    inline bool isExpanded() { return m_expanded; };
    inline bool isDeleted() { return m_deleted; };

    inline void setSelected( bool val ) { m_selected = val; }
    inline void setPlaying( bool val ) { m_playing = val; }
    inline void setExpanded( bool val ) { m_expanded = val; }
    inline void setDeleted( bool val ) { m_deleted = val; }

    inline void toggleSelected() { m_selected = !m_selected; }
    inline void toggleExpanded() { m_expanded = !m_expanded; }

    /// Get the number of children
    int size() const { return m_children.size(); }

    /// Iterators
    typedef list<VarTree>::iterator Iterator;
    typedef list<VarTree>::const_iterator ConstIterator;

    /// Begining of the children's list
    Iterator begin() { return m_children.begin(); }
    ConstIterator begin() const { return m_children.begin(); }

    /// End of children's list
    Iterator end() { return m_children.end(); }
    ConstIterator end() const { return m_children.end(); }

    /// Back of children's list
    VarTree &back() { return m_children.back(); }

    /// Return an iterator on the n'th element of the children's list
    Iterator operator[]( int n );
    ConstIterator operator[]( int n ) const;

    /// Parent node
    VarTree *parent() { return m_pParent; }

    /// Get next sibling
    Iterator getNextSiblingOrUncle();

    Iterator next_uncle();
    Iterator prev_uncle();

    /// Get first leaf
    Iterator firstLeaf();

    /// Remove a child
    void removeChild( Iterator it ) { m_children.erase( it ); }

    /// Execute the action associated to this item
    virtual void action( VarTree *pItem ) { }

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

    /// return rank of visible item starting from 1
    int getRank( const Iterator& it, bool flat );

    /// Find a children node with the given id
    Iterator findById( int id );

    /// Ensure an item is expanded
    void ensureExpanded( const Iterator& it );

    /// flag a whole subtree for deletion
    void cascadeDelete();

    /// Get depth (root depth is 0)
    int depth()
    {
        VarTree *parent = this;
        int depth = 0;
        while( ( parent = parent->parent() ) != NULL )
            depth++;
        return depth;
    }

private:

    /// Get root node
    VarTree *root()
    {
        VarTree *parent = this;
        while( parent->parent() != NULL )
            parent = parent->parent();
        return parent;
    }

    /// List of children
    list<VarTree> m_children;

    /// Pointer to parent node
    VarTree *m_pParent;

    int m_id;
    void *m_pData;
    UStringPtr m_cString;

    /// indicators
    bool m_readonly;
    bool m_selected;
    bool m_playing;
    bool m_expanded;
    bool m_deleted;

    /// Variable type
    static const string m_type;

    /// Position variable
    VariablePtr m_cPosition;
};

#endif
