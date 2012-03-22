/*****************************************************************************
 * ctrl_tree.hpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
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

#ifndef CTRL_TREE_HPP
#define CTRL_TREE_HPP

#include "ctrl_generic.hpp"
#include "../utils/observer.hpp"
#include "../utils/var_tree.hpp"

class OSGraphics;
class GenericFont;
class GenericBitmap;

/// Class for control tree
class CtrlTree: public CtrlGeneric, public Observer<VarTree, tree_update>
{
public:
    typedef VarTree::IteratorVisible Iterator;

    CtrlTree( intf_thread_t *pIntf,
              VarTree &rTree,
              const GenericFont &rFont,
              const GenericBitmap *pBgBitmap,
              const GenericBitmap *pItemBitmap,
              const GenericBitmap *pOpenBitmap,
              const GenericBitmap *pClosedBitmap,
              uint32_t fgColor,
              uint32_t playColor,
              uint32_t bgColor1,
              uint32_t bgColor2,
              uint32_t selColor,
              const UString &rHelp,
              VarBool *pVisible,
              VarBool *pFlat );
    virtual ~CtrlTree();

    /// Handle an event on the control
    virtual void handleEvent( EvtGeneric &rEvent );

    /// Check whether coordinates are inside the control
    virtual bool mouseOver( int x, int y ) const;

    /// Draw the control on the given graphics
    virtual void draw( OSGraphics &rImage, int xDest, int yDest, int w, int h );

    /// Called when the layout is resized
    virtual void onResize();

    /// Return true if the control can gain the focus
    virtual bool isFocusable() const { return true; }

    /// Return true if the control can be scrollable
    virtual bool isScrollable() const { return true; }

    /// Get the type of control (custom RTTI)
    virtual string getType() const { return "tree"; }

    /// Make sure an item is visible
    /// \param item an iterator to a tree item
    /// \return true if it changed the position
    bool ensureVisible( const Iterator& it );

private:
    /// Tree associated to the control
    VarTree &m_rTree;
    /// Font
    const GenericFont &m_rFont;
    /// Background bitmap
    const GenericBitmap *m_pBgBitmap;
    /// Item (leaf) bitmap
    // (TODO : add different bitmaps for different item types
    //         like in the wx playlist)
    const GenericBitmap *m_pItemBitmap;
    /// Open (expanded) node bitmap
    const GenericBitmap *m_pOpenBitmap;
    /// Closed node bitmap
    const GenericBitmap *m_pClosedBitmap;
    /// scaled bitmap
    GenericBitmap *m_pScaledBitmap;
    /// Image of the control
    OSGraphics *m_pImage;

    /// Color of normal test
    uint32_t m_fgColor;
    /// Color of the playing item
    uint32_t m_playColor;
    /// Background colors, used when no background bitmap is given
    uint32_t m_bgColor1, m_bgColor2;
    /// Background of selected items
    uint32_t m_selColor;

    /// First item in the visible area
    Iterator m_firstPos;
    /// Pointer on the last clicked item in the tree
    Iterator m_lastClicked;
    ///
    Iterator m_itOver;

    /// Do we want to "flaten" the tree ?
    bool m_flat;
    /// Number of visible lines
    float m_capacity;
    /// flag for item deletion
    bool m_bRefreshOnDelete;

    /// Method called when the tree variable is modified
    virtual void onUpdate( Subject<VarTree, tree_update> &rTree,
                           tree_update *);

    /// Called when the position is set
    virtual void onPositionChange();

    /// Compute the number of lines that can be displayed
    float maxItems();

    /// Compute the item's height (depends on fonts and images used)
    int itemHeight();

    /// Compute the width of an item's bitmap
    int itemImageWidth();

    /// Draw the image of the control
    void makeImage();

    /// Return the n'th displayed item (starting at position 0)
    Iterator findItemAtPos( int n );

    /// return the nearest item
    Iterator getNearestItem( const Iterator& it );

    /// return whether the item is visible or not
    bool isItemVisible( const Iterator& it );

    void setSliderFromFirst();
    Iterator getFirstFromSlider();
    void setScrollStep();
};

#endif
