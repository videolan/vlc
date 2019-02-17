/*****************************************************************************
 * os2_window.hpp
 *****************************************************************************
 * Copyright (C) 2003, 2013 the VideoLAN team
 *
 * Authors: Cyril Deguet      <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
 *          KO Myung-Hun      <komh@chollian.net>
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

#ifndef OS2_WINDOW_HPP
#define OS2_WINDOW_HPP

#include "../src/generic_window.hpp"
#include "../src/os_window.hpp"

#include <vlc_vout_window.h>


/// OS2 implementation of OSWindow
class OS2Window: public OSWindow
{
public:
    OS2Window( intf_thread_t *pIntf, GenericWindow &rWindow,
               HMODULE hInst, HWND hParentWindow,
               bool dragDrop, bool playOnDrop,
               OS2Window *pParentWindow, GenericWindow::WindowType_t );
    virtual ~OS2Window();

    // Show the window
    virtual void show() const;

    // Hide the window
    virtual void hide() const;

    /// Move and resize the window
    virtual void moveResize( int left, int top, int width, int height ) const;

    /// Bring the window on top
    virtual void raise() const;

    /// Set the opacity of the window (0 = transparent, 255 = opaque)
    virtual void setOpacity( uint8_t value ) const;

    /// Toggle the window on top
    virtual void toggleOnTop( bool onTop ) const;

    /// Getter for the window handle
    HWND getHandle() const { return m_hWndClient; }

    /// Set the window handler
    void setOSHandle( vout_window_t *pWnd ) const {
        pWnd->type = VOUT_WINDOW_TYPE_HWND;
        pWnd->info.has_double_click = true;
        pWnd->handle.hwnd = ( void * )getHandle();
    }

    /// reparent the window
    void reparent( OSWindow *parent, int x, int y, int w, int h );

    /// invalidate a window surface
    bool invalidateRect( int x, int y, int w, int h ) const;

private:
    /// Window handle
    HWND m_hWnd;
    /// Client window handle
    HWND m_hWndClient;
    /// Window parent's handle
    HWND m_hWnd_parent;
    /// Indicates whether the window handles drag&drop events
    bool m_dragDrop;
    /// Indicates whether the window is layered
    mutable bool m_isLayered;
    /// Parent window
    OS2Window *m_pParent;
    /// window type
    GenericWindow::WindowType_t m_type;
};


#endif
