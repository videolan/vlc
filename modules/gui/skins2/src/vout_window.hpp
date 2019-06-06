/*****************************************************************************
 * vout_window.hpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
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

#ifndef VOUT_WINDOW_HPP
#define VOUT_WINDOW_HPP

#include "generic_window.hpp"
#include "dialogs.hpp"
#include "../commands/cmd_generic.hpp"
#include <vlc_vout_window.h>

class OSGraphics;
class OSTimer;
class CtrlVideo;
struct vout_window_t;

/// Class to handle a video output window
class VoutWindow: private GenericWindow
{
public:

    VoutWindow( intf_thread_t *pIntf, struct vout_window_t* pWnd,
                int width, int height, GenericWindow* pParent = NULL );
    virtual ~VoutWindow();

    /// Make some functions public
    //@{
    using GenericWindow::show;
    using GenericWindow::hide;
    using GenericWindow::move;
    using GenericWindow::resize;
    using GenericWindow::updateWindowConfiguration;
    using GenericWindow::getMonitorInfo;
    //@}

    /// get the parent  window
    virtual GenericWindow* getWindow( ) { return m_pParentWindow; }

    /// hotkeys processing
    virtual void processEvent( EvtKey &rEvtKey );
    virtual void processEvent( EvtScroll &rEvtScroll );
    virtual void processEvent( EvtMotion &rEvtMotion );
    virtual void processEvent( EvtMouse &rEvtMouse );

    /// set and get Video Control for VoutWindow
    virtual void setCtrlVideo( CtrlVideo* pCtrlVideo );
    virtual CtrlVideo* getCtrlVideo( ) { return m_pCtrlVideo; }

    /// get original size of vout
    virtual int getOriginalWidth( ) { return original_width; }
    virtual int getOriginalHeight( ) { return original_height; }

    /// set original size of vout
    virtual void setOriginalWidth( int width ) { original_width = width; }
    virtual void setOriginalHeight( int height ) { original_height = height; }

    /// Resize the window
    virtual void resize( int width, int height );

    // Hide/show cursor
    void showMouse( );
    void hideMouse( bool );

    virtual std::string getType() const { return "Vout"; }

private:

    /// vout thread
    struct vout_window_t* m_pWnd;

    /// original width and height
    int original_width;
    int original_height;

    /// VideoControl attached to it
    CtrlVideo* m_pCtrlVideo;

    /// Parent Window
    GenericWindow* m_pParentWindow;

    // Cursor timer
    OSTimer *m_pTimer;
    int mouse_hide_timeout;
    DEFINE_CALLBACK( VoutWindow, HideMouse );
};

typedef CountedPtr<VoutWindow> VoutWindowPtr;

#endif
