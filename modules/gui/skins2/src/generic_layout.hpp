/*****************************************************************************
 * generic_layout.hpp
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

#ifndef GENERIC_LAYOUT_HPP
#define GENERIC_LAYOUT_HPP

#include "skin_common.hpp"
#include "top_window.hpp"
#include "../utils/pointer.hpp"
#include "../utils/position.hpp"

#include <list>

class Anchor;
class OSGraphics;
class CtrlGeneric;
class CtrlVideo;
class VarBoolImpl;


/// Control and its associated layer
struct LayeredControl
{
    LayeredControl( CtrlGeneric *pControl, int layer ):
        m_pControl( pControl ), m_layer( layer ) { }

    /// Pointer on the control
    CtrlGeneric *m_pControl;
    /// Layer number
    int m_layer;
};


/// Base class for layouts
class GenericLayout: public SkinObject
{
public:
    GenericLayout( intf_thread_t *pIntf, int width, int height,
                   int minWidth, int maxWidth, int minHeight, int maxHeight );

    virtual ~GenericLayout();

    /// Attach the layout to a window
    virtual void setWindow( TopWindow *pWindow );

    /// Get the associated window, if any
    virtual TopWindow *getWindow() const { return m_pWindow; }

    /// Called by a control which wants to capture the mouse
    virtual void onControlCapture( const CtrlGeneric &rCtrl );

    /// Called by a control which wants to release the mouse
    virtual void onControlRelease( const CtrlGeneric &rCtrl );

    /// Refresh the window
    virtual void refreshAll();

    /// Refresh a rectangular portion of the window
    virtual void refreshRect( int x, int y, int width, int height );

    /// Get the image of the layout
    virtual OSGraphics *getImage() const { return m_pImage; }

    /// Get the position of the layout (relative to the screen)
    /**
     * Note: These values are different from the m_rect.getLeft() and
     * m_rect.getTop(), which always return 0.
     * The latter methods are there as a "root rect" for the panels and
     * controls, since each control knows its parent rect, but returns
     * coordinates relative to the root rect.
     */
    virtual int getLeft() const { return m_pWindow->getLeft(); }
    virtual int getTop() const { return m_pWindow->getTop(); }

    /// Get the size of the layout
    virtual int getWidth() const { return m_rect.getWidth(); }
    virtual int getHeight() const { return m_rect.getHeight(); }
    virtual const GenericRect &getRect() const { return m_rect; }

    /// Get the minimum and maximum size of the layout
    virtual int getMinWidth() const { return m_minWidth; }
    virtual int getMaxWidth() const { return m_maxWidth; }
    virtual int getMinHeight() const { return m_minHeight; }
    virtual int getMaxHeight() const { return m_maxHeight; }

    /// Resize the layout
    virtual void resize( int width, int height );

    /// determine whether layouts should be kept the same size
    virtual bool isTightlyCoupledWith( const GenericLayout& otherLayout ) const;

    // getter for layout visibility
    virtual bool isVisible( ) const { return m_visible; }

    /**
     * Add a control in the layout at the given position, and
     * the optional given layer
     */
    virtual void addControl( CtrlGeneric *pControl,
                             const Position &rPosition,
                             int layer );

    /// Get the list of the controls in this layout, by layer order
    virtual const std::list<LayeredControl> &getControlList() const;

    /// Called by a control when its image has changed
    /**
     * The arguments indicate the size of the rectangle to refresh,
     * and the offset (from the control position) of this rectangle.
     * Use a negative width or height to refresh the layout completely
     */
    virtual void onControlUpdate( const CtrlGeneric &rCtrl,
                                  int width, int height,
                                  int xOffSet, int yOffSet );

    /// Get the list of the anchors of this layout
    virtual const std::list<Anchor*>& getAnchorList() const;

    /// Add an anchor to this layout
    virtual void addAnchor( Anchor *pAnchor );

    /// Called when the layout is shown
    virtual void onShow();

    /// Called when the layout is hidden
    virtual void onHide();

    /// Give access to the "active layout" variable
    // FIXME: we give read/write access
    VarBoolImpl &getActiveVar() { return *m_pVarActive; }

private:
    /// Parent window of the layout
    TopWindow *m_pWindow;
    /// Layout original size
    const int m_original_width;
    const int m_original_height;
    /// Layout size
    SkinsRect m_rect;
    const int m_minWidth, m_maxWidth;
    const int m_minHeight, m_maxHeight;
    /// Image of the layout
    OSGraphics *m_pImage;
    /// List of the controls in the layout
    std::list<LayeredControl> m_controlList;
    /// Video control(s)
    std::set<CtrlVideo *> m_pVideoCtrlSet;
    /// List of the anchors in the layout
    std::list<Anchor*> m_anchorList;
    /// Flag to know if the layout is visible
    bool m_visible;
    /// Variable for the "active state" of the layout
    /**
     * Note: the layout is not an observer on this variable, because it
     * cannot be changed externally (i.e. without an explicit change of
     * layout). This way, we avoid using a setActiveLayoutInner method.
     */
    mutable VarBoolImpl *m_pVarActive;
};


typedef CountedPtr<GenericLayout> GenericLayoutPtr;


#endif
