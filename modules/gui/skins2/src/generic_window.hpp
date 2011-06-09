/*****************************************************************************
 * generic_window.hpp
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

#ifndef GENERIC_WINDOW_HPP
#define GENERIC_WINDOW_HPP

#include "skin_common.hpp"
#include "../utils/var_bool.hpp"

class OSWindow;
class EvtGeneric;
class EvtFocus;
class EvtLeave;
class EvtMenu;
class EvtMotion;
class EvtMouse;
class EvtKey;
class EvtRefresh;
class EvtScroll;
class EvtDragEnter;
class EvtDragLeave;
class EvtDragOver;
class EvtDragDrop;
class WindowManager;


/// Generic window class
class GenericWindow: public SkinObject, public Observer<VarBool>
{
private:
    friend class WindowManager;
    friend class VoutManager;
    friend class CtrlVideo;
public:

    enum WindowType_t
    {
        TopWindow,
        VoutWindow,
        FullscreenWindow,
        FscWindow,
    };

    GenericWindow( intf_thread_t *pIntf, int xPos, int yPos,
                   bool dragDrop, bool playOnDrop,
                   GenericWindow *pParent = NULL,
                   WindowType_t type = TopWindow );
    virtual ~GenericWindow();

    /// Methods to process OS events.
    virtual void processEvent( EvtFocus &rEvtFocus ) { (void)rEvtFocus; }
    virtual void processEvent( EvtMenu &rEvtMenu ) { (void)rEvtMenu; }
    virtual void processEvent( EvtMotion &rEvtMotion ) { (void)rEvtMotion; }
    virtual void processEvent( EvtMouse &rEvtMouse ) { (void)rEvtMouse; }
    virtual void processEvent( EvtLeave &rEvtLeave ) { (void)rEvtLeave; }
    virtual void processEvent( EvtKey &rEvtKey ) { (void)rEvtKey; }
    virtual void processEvent( EvtScroll &rEvtScroll ) { (void)rEvtScroll; }

    virtual void processEvent( EvtDragEnter &rEvtDragEnter )
        { (void)rEvtDragEnter; }
    virtual void processEvent( EvtDragLeave &rEvtDragLeave )
        { (void)rEvtDragLeave; }
    virtual void processEvent( EvtDragOver &rEvtDragOver )
        { (void)rEvtDragOver; }
    virtual void processEvent( EvtDragDrop &rEvtDragDrop )
        { (void)rEvtDragDrop; }

    virtual void processEvent( EvtRefresh &rEvtRefresh );

    /// Resize the window
    virtual void resize( int width, int height );

    /// Refresh an area of the window
    virtual void refresh( int left, int top, int width, int height )
        { (void)left; (void)top; (void)width; (void)height; }

    /// Invalidate an area of the window
    virtual void invalidateRect( int left, int top, int width, int height );

    /// Get the coordinates of the window
    int getLeft() const { return m_left; }
    int getTop() const { return m_top; }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    void getMonitorInfo( int* x, int* y, int* width, int* height ) const;

    /// Give access to the visibility variable
    VarBool &getVisibleVar() { return *m_pVarVisible; }

    /// Window type, mainly useful when overloaded (for VoutWindow)
    virtual string getType() const { return "Generic"; }

    /// windows handle
    vlc_wnd_type getOSHandle() const;

    /// reparent
    void setParent( GenericWindow* pParent,
                    int x = 0, int y = 0, int w = -1, int h = -1 );

protected:
    /// Get the OS window
    OSWindow *getOSWindow() const { return m_pOsWindow; }

    /// These methods do not need to be public since they are accessed
    /// only by the window manager or by inheritant classes.
    //@{
    /// Show the window
    virtual void show() const;

    /// Hide the window
    virtual void hide() const;

    /// Move the window
    virtual void move( int left, int top );

    /// Bring the window on top
    virtual void raise() const;

    /// Set the opacity of the window (0 = transparent, 255 = opaque)
    virtual void setOpacity( uint8_t value );

    /// Toggle the window on top
    virtual void toggleOnTop( bool onTop ) const;
    //@}

    /// Actually show the window
    virtual void innerShow();

    /// Actually hide the window
    virtual void innerHide();

    ///
    bool isVisible() const { return m_pVarVisible->get(); }

    /// Method called when the observed variable is modified
    virtual void onUpdate( Subject<VarBool> &rVariable , void*);

private:
    /// Window position and size
    int m_left, m_top, m_width, m_height;
    /// OS specific implementation
    OSWindow *m_pOsWindow;
    /// Variable for the visibility of the window
    mutable VarBoolImpl *m_pVarVisible;
};


#endif
