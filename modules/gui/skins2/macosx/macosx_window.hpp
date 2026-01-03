/*****************************************************************************
 * macosx_window.hpp
 *****************************************************************************
 * Copyright (C) 2024 the VideoLAN team
 *
 * Authors: VLC contributors
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

#ifndef MACOSX_WINDOW_HPP
#define MACOSX_WINDOW_HPP

#include "../src/generic_window.hpp"
#include "../src/os_window.hpp"

#include <vlc_window.h>

#ifdef __OBJC__
@class NSWindow;
@class VLCSkinsWindow;
#else
typedef void NSWindow;
typedef void VLCSkinsWindow;
#endif

class MacOSXDragDrop;

/// macOS implementation of OSWindow
class MacOSXWindow: public OSWindow
{
public:
    MacOSXWindow( intf_thread_t *pIntf, GenericWindow &rWindow,
                  bool dragDrop, bool playOnDrop,
                  MacOSXWindow *pParentWindow,
                  GenericWindow::WindowType_t type );

    virtual ~MacOSXWindow();

    // Show the window
    virtual void show() const;

    // Hide the window
    virtual void hide() const;

    /// Move and resize the window
    virtual void moveResize( int left, int top,
                             int width, int height ) const;

    /// Bring the window on top
    virtual void raise() const;

    /// Set the opacity of the window (0 = transparent, 255 = opaque)
    virtual void setOpacity( uint8_t value ) const;

    /// Toggle the window on top
    virtual void toggleOnTop( bool onTop ) const;

    /// Set the window handler for video output
    virtual void setOSHandle( vlc_window_t *pWnd ) const;

    /// Reparent the window
    virtual void reparent( OSWindow *pParent, int x, int y, int w, int h );

    /// Invalidate a window region
    virtual bool invalidateRect( int x, int y, int w, int h ) const;

    /// Get the NSWindow
    NSWindow *getNSWindow() const { return m_pWindow; }

    /// Get the associated GenericWindow
    GenericWindow &getGenericWindow() const { return m_rWindow; }

private:
    /// Associated GenericWindow
    GenericWindow &m_rWindow;
    /// NSWindow handle
    VLCSkinsWindow *m_pWindow;
    /// Parent window
    MacOSXWindow *m_pParent;
    /// Window type
    GenericWindow::WindowType_t m_type;
    /// Drag & drop handler
    MacOSXDragDrop *m_pDropTarget;
    /// Screen height (for coordinate conversion)
    int m_screenHeight;
};

#endif
