/*****************************************************************************
 * macosx_popup.hpp
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

#ifndef MACOSX_POPUP_HPP
#define MACOSX_POPUP_HPP

#include "../src/os_popup.hpp"


/// MacOSX implementation of OSPopup
class MacOSXPopup: public OSPopup
{
public:
    MacOSXPopup( intf_thread_t *pIntf );

    virtual ~MacOSXPopup();

    /// Show the popup menu at the given (absolute) corrdinates
    virtual void show( int xPos, int yPos );

    /// Hide the popup menu
    virtual void hide();

    /// Append a new menu item with the given label to the popup menu
    virtual void addItem( const string &rLabel, int pos );

    /// Create a dummy menu item to separate sections
    virtual void addSeparator( int pos );

    /// Return the position of the item identified by the given id
    virtual int getPosFromId( int id ) const;
};


#endif
