/*****************************************************************************
 * evt_menu.hpp
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

#ifndef EVT_MENU_HPP
#define EVT_MENU_HPP

#include "evt_generic.hpp"


/// Mouse move event
class EvtMenu: public EvtGeneric
{
public:
    EvtMenu( intf_thread_t *pIntf, int itemId )
           : EvtGeneric( pIntf ), m_itemId( itemId ) { }
    virtual ~EvtMenu() { }
    virtual const std::string getAsString() const { return "menu"; }

    int getItemId() const { return m_itemId; }

private:
    /// Coordinates of the mouse (absolute or relative)
    int m_itemId;
};


#endif
