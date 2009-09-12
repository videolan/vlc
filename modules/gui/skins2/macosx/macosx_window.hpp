/*****************************************************************************
 * macosx_window.hpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
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

#ifndef MACOSX_WINDOW_HPP
#define MACOSX_WINDOW_HPP

#include "../src/os_window.hpp"
#include <Carbon/Carbon.h>

class MacOSXDisplay;
class MacOSXDragDrop;


/// MacOSX implementation of OSWindow
class MacOSXWindow: public OSWindow
{
public:
    MacOSXWindow( intf_thread_t *pIntf, GenericWindow &rWindow,
                  bool dragDrop, bool playOnDrop,
                  MacOSXWindow *pParentWindow );

    virtual ~MacOSXWindow();

    // Show the window
    virtual void show( int left, int top ) const;

    // Hide the window
    virtual void hide() const;

    /// Move the window
    virtual void moveResize( int left, int top,
                             int width, int height ) const;

    /// Bring the window on top
    virtual void raise() const;

    /// Set the opacity of the window (0 = transparent, 255 = opaque)
    virtual void setOpacity( uint8_t value ) const;

    /// Toggle the window on top
    virtual void toggleOnTop( bool onTop ) const;

    /// Get the Carbon window handle
    WindowRef getWindowRef() const { return m_win; };

private:
    /// Parent window
    MacOSXWindow *m_pParent;
    /// Indicates whether the window handles drag&drop events
    bool m_dragDrop;
    /// Carbon Window object
    WindowRef m_win;
};


#endif
