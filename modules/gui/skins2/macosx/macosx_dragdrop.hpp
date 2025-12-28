/*****************************************************************************
 * macosx_dragdrop.hpp
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

#ifndef MACOSX_DRAGDROP_HPP
#define MACOSX_DRAGDROP_HPP

#include "../src/skin_common.hpp"
#include "../src/generic_window.hpp"

#ifdef __OBJC__
@class NSWindow;
@class VLCDropView;
#else
typedef void NSWindow;
typedef void VLCDropView;
#endif

/// macOS drag and drop handler
class MacOSXDragDrop: public SkinObject
{
public:
    MacOSXDragDrop( intf_thread_t *pIntf, NSWindow *pWindow,
                    bool playOnDrop, GenericWindow *pWin );
    virtual ~MacOSXDragDrop();

    /// Handle dropped files
    void handleDrop( const char **files, int count );

    /// Check if files should play immediately
    bool getPlayOnDrop() const { return m_playOnDrop; }

    /// Get the associated GenericWindow
    GenericWindow *getWindow() const { return m_pWin; }

private:
    /// Window
    NSWindow *m_pWindow;
    /// Drop view
    VLCDropView *m_pDropView;
    /// Should dropped files play immediately
    bool m_playOnDrop;
    /// Associated generic window
    GenericWindow *m_pWin;
};

#endif
