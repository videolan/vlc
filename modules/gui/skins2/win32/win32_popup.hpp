/*****************************************************************************
 * win32_popup.hpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Olivier Teulière <ipkiss@via.ecp.fr>
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

#ifndef WIN32_POPUP_HPP
#define WIN32_POPUP_HPP

#include "../src/os_popup.hpp"
#include <windows.h>


/// Win32 implementation of OSPopup
class Win32Popup: public OSPopup
{
public:
    Win32Popup( intf_thread_t *pIntf, HWND hAssociatedWindow );

    virtual ~Win32Popup();

    /// Show the popup menu at the given (absolute) corrdinates
    virtual void show( int xPos, int yPos );

    /// Hide the popup menu
    virtual void hide();

    /// Append a new menu item with the given label to the popup menu
    virtual void addItem( const string &rLabel, int pos );

    /// Create a dummy menu item to separate sections
    virtual void addSeparator( int pos );

    /// Return the position of the item identified by the given id
    virtual int getPosFromId( int id ) const { return id; }

private:
    /// Menu handle
    HMENU m_hMenu;
    /// Handle of the window which will receive the menu events
    HWND m_hWnd;

    /**
     * Find the item before which to insert another item so that the
     * newly added item is at the position pos _when the whole menu has
     * been built_ (no assumption is made for the order of insertion of
     * the items)
     */
    unsigned int findInsertionPoint( unsigned int pos ) const;
};


#endif
