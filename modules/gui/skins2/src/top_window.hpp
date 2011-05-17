/*****************************************************************************
 * top_window.hpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
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

#ifndef TOP_WINDOW_HPP
#define TOP_WINDOW_HPP

#include "generic_window.hpp"
#include "../utils/pointer.hpp"
#include <list>

class OSWindow;
class OSGraphics;
class GenericLayout;
class CtrlGeneric;
class WindowManager;


/// Class to handle top-level windows
class TopWindow: public GenericWindow
{
private:
    friend class WindowManager;
public:
    TopWindow( intf_thread_t *pIntf, int xPos, int yPos,
               WindowManager &rWindowManager,
               bool dragDrop, bool playOnDrop, bool visible,
               GenericWindow::WindowType_t type = GenericWindow::TopWindow );
    virtual ~TopWindow();

    /// Methods to process OS events.
    virtual void processEvent( EvtFocus &rEvtFocus );
    virtual void processEvent( EvtMenu &rEvtMenu );
    virtual void processEvent( EvtMotion &rEvtMotion );
    virtual void processEvent( EvtMouse &rEvtMouse );
    virtual void processEvent( EvtLeave &rEvtLeave );
    virtual void processEvent( EvtKey &rEvtKey );
    virtual void processEvent( EvtScroll &rEvtScroll );
    virtual void processEvent( EvtDragDrop &rEvtDragDrop );
    virtual void processEvent( EvtDragOver &rEvtDragOver );
    virtual void processEvent( EvtDragLeave &rEvtDragLeave );

    /// Forward an event to a control
    virtual void forwardEvent( EvtGeneric &rEvt, CtrlGeneric &rCtrl );

    // Refresh an area of the window
    virtual void refresh( int left, int top, int width, int height );

    /// Get the active layout
    virtual const GenericLayout& getActiveLayout() const;

    /// Update the shape of the window from the active layout
    virtual void updateShape();

    /// Called by a control that wants to capture the mouse
    virtual void onControlCapture( const CtrlGeneric &rCtrl );

    /// Called by a control that wants to release the mouse
    virtual void onControlRelease( const CtrlGeneric &rCtrl );

    /// Called by a control when its tooltip changed
    virtual void onTooltipChange( const CtrlGeneric &rCtrl );

    /// Get the "maximized" variable
    VarBool &getMaximizedVar() { return *m_pVarMaximized; }

    /// Get the initial visibility status
    bool getInitialVisibility() const { return m_initialVisibility; }

protected:
    /// Actually show the window
    virtual void innerShow();

    /// Actually hide the window
    virtual void innerHide();

private:
    /**
     * These methods are only used by the window manager
     */
    //@{
    /// Change the active layout
    virtual void setActiveLayout( GenericLayout *pLayout );
    //@}

    /// Initial visibility status
    bool m_initialVisibility;
    /// indicator if playback is requested on drag&drop
    bool m_playOnDrop;
    /// Window manager
    WindowManager &m_rWindowManager;
    /// Current active layout of the window
    GenericLayout *m_pActiveLayout;
    /// Last control on which the mouse was over
    CtrlGeneric *m_pLastHitControl;
    /// Control that has captured the mouse
    CtrlGeneric *m_pCapturingControl;
    /// Control that has the focus
    CtrlGeneric *m_pFocusControl;
    /// Control over which drag&drop is hovering
    CtrlGeneric *m_pDragControl;
    /// Current key modifier (also used for mouse)
    int m_currModifier;

    /// Variable for the visibility of the window
    VarBoolImpl *m_pVarMaximized;

    /**
     * Find the uppest control in the layout hit by the mouse, and send
     * it an enter event if needed
     */
    CtrlGeneric *findHitControl( int xPos, int yPos );

    /**
     * Update the lastHitControl pointer and send a leave event to the
     * right control
     */
    void setLastHit( CtrlGeneric *pNewHitControl );
};

typedef CountedPtr<TopWindow> TopWindowPtr;


#endif
