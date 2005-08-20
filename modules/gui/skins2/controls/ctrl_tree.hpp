/*****************************************************************************
 * ctrl_tree.hpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: ctrl_list.hpp 11009 2005-05-14 14:39:05Z ipkiss $
 *
 * Authors: Antoine Cellerier
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

#ifndef CTRL_TREE_HPP
#define CTRL_TREE_HPP

#include "ctrl_generic.hpp"
#include "../utils/observer.hpp"
#include "../utils/var_tree.hpp"

class OSGraphics;
class GenericFont;
class GenericBitmap;

/// Class for control tree
class CtrlTree: public CtrlGeneric, public Observer<VarTree>,
    public Observer<VarPercent>
{
    public:
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
                  VarBool *pVisible );
        virtual ~CtrlTree();

        /// Handle an event on the control
        virtual void handleEvent( EvtGeneric &rEvent );

        /// Check whether coordinates are inside the control
        virtual bool mouseOver( int x, int y ) const;

        /// Draw the control on the given graphics
        virtual void draw( OSGraphics &rImage, int xDest, int yDest );

        /// Called when the layout is resized
        virtual void onResize();

        /// Return true if the control can gain the focus
        virtual bool isFocusable() const { return true; }

        /// Get the type of control (custom RTTI)
        virtual string getType() const { return "tree"; }

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
        /// Color of normal test
        uint32_t m_fgColor;
        /// Color of the playing item
        uint32_t m_playColor;
        /// Background colors, used when no background bitmap is given
        uint32_t m_bgColor1, m_bgColor2;
        /// Background of selected items
        uint32_t m_selColor;
        /// Pointer on the last selected item in the tree
        VarTree *m_pLastSelected;
        /// Image of the control
        OSGraphics *m_pImage;
        /// Last position
        VarTree::Iterator m_lastPos;

        /// Method called when the tree variable is modified
        virtual void onUpdate( Subject<VarTree> &rTree );

        // Method called when the position variable of the tree is modified
        virtual void onUpdate( Subject<VarPercent> &rPercent );

        /// Called when the position is set
        virtual void onPositionChange();

        /// Compute the number of lines that can be displayed
        int maxItems();

        /// Compute the item's height (depends on fonts and images used)
        int itemHeight();

        /// Compute the width of an item's bitmap
        int itemImageWidth();

        /// Check if the tree must be scrolled
        void autoScroll();

        /// Draw the image of the control
        void makeImage();
};

#endif
