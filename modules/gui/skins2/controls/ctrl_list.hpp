/*****************************************************************************
 * ctrl_list.hpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
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

#ifndef CTRL_LIST_HPP
#define CTRL_LIST_HPP

#include "ctrl_generic.hpp"
#include "../utils/observer.hpp"
#include "../utils/var_list.hpp"

class OSGraphics;
class GenericFont;
class GenericBitmap;


/// Class for control list
class CtrlList: public CtrlGeneric, public Observer<VarList>,
    public Observer<VarPercent>
{
public:
    CtrlList( intf_thread_t *pIntf, VarList &rList,
              const GenericFont &rFont, const GenericBitmap *pBitmap,
              uint32_t fgcolor, uint32_t playcolor, uint32_t bgcolor1,
              uint32_t bgcolor2, uint32_t selColor,
              const UString &rHelp, VarBool *pVisible );
    virtual ~CtrlList();

    /// Handle an event on the control.
    virtual void handleEvent( EvtGeneric &rEvent );

    /// Check whether coordinates are inside the control.
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
    virtual std::string getType() const { return "list"; }

private:
    /// List associated to the control
    VarList &m_rList;
    /// Font
    const GenericFont &m_rFont;
    /// Background bitmap
    /** If NULL, the 2 background colors defined below will be used */
    const GenericBitmap *m_pBitmap;
    /// Color of normal text
    uint32_t m_fgColor;
    /// Color of the playing item
    uint32_t m_playColor;
    /// Background colors, used when no background bitmap is given
    uint32_t m_bgColor1, m_bgColor2;
    /// Background of selected items
    uint32_t m_selColor;
    /// Pointer on the last selected item in the list
    VarList::Elem_t *m_pLastSelected;
    /// Image of the control
    OSGraphics *m_pImage;
    /// Last position
    int m_lastPos;

    /// Method called when the list variable is modified
    virtual void onUpdate( Subject<VarList> &rList, void* );

    /// Method called when the position variable of the list is modified
    virtual void onUpdate( Subject<VarPercent> &rPercent, void*  );

    /// Called when the position is set
    virtual void onPositionChange();

    /// Check if the list must be scrolled
    void autoScroll();

    /// Draw the image of the control
    void makeImage();
};

#endif
