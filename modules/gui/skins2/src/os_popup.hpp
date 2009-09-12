/*****************************************************************************
 * os_popup.hpp
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

#ifndef OS_POPUP_HPP
#define OS_POPUP_HPP

#include "skin_common.hpp"
#include <string>

class OSGraphics;


/// Base class for OS specific Popup Windows
class OSPopup: public SkinObject
{
public:
    virtual ~OSPopup() { }

    /// Show the popup menu at the given (absolute) corrdinates
    virtual void show( int xPos, int yPos ) = 0;

    /// Hide the popup menu
    virtual void hide() = 0;

    /// Append a new menu item with the given label to the popup menu
    virtual void addItem( const string &rLabel, int pos ) = 0;

    /// Create a dummy menu item to separate sections
    virtual void addSeparator( int pos ) = 0;

    /// Return the position of the item identified by the given id
    virtual int getPosFromId( int id ) const = 0;

protected:
    OSPopup( intf_thread_t *pIntf ): SkinObject( pIntf ) { }
};

#endif
