/*****************************************************************************
 * popup.hpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
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

#ifndef POPUP_HPP
#define POPUP_HPP

#include "skin_common.hpp"
#include "../utils/pointer.hpp"
#include <string>
#include <map>

class OSPopup;
class TopWindow;
class CmdGeneric;
class WindowManager;
class EvtMenu;


/// Class handling a popup menu
class Popup: public SkinObject
{
public:
    Popup( intf_thread_t *pIntf, WindowManager &rWindowManager );
    virtual ~Popup() { }

    void show( int xPos, int yPos );
    void hide();

    // XXX: it would be nice to use a UString here, if X11 supports it for
    // its menu items (Windows doesn't, unfortunately)
    /// Insert a new menu item to the popup menu, at the position pos
    void addItem( const std::string &rLabel, CmdGeneric &rCmd, int pos );

    /// Create a dummy menu item to separate sections
    void addSeparator( int pos );

    /// Execute the action corresponding to the chosen menu item
    void handleEvent( const EvtMenu &rEvent );

private:
    /// OS specific implementation
    OSPopup *m_pOsPopup;

    /// Window manager
    WindowManager &m_rWindowManager;

    /// Actions for the menu items, indexed by the position in the menu
    std::map<int, CmdGeneric *> m_actions;
};

typedef CountedPtr<Popup> PopupPtr;


#endif
