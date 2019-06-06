/*****************************************************************************
 * evt_refresh.hpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
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

#ifndef EVT_REFRESH_HPP
#define EVT_REFRESH_HPP

#include "evt_generic.hpp"


/// Refresh window event
class EvtRefresh: public EvtGeneric
{
public:
    /// Constructor with the coordinates of the area to refresh
    EvtRefresh( intf_thread_t *pIntf, int xStart, int yStart,
                                      int width, int height )
              : EvtGeneric( pIntf ), m_xStart( xStart ), m_yStart( yStart ),
                                     m_width( width ), m_height( height ) { }

    virtual ~EvtRefresh() { }
    virtual const std::string getAsString() const { return "refresh"; }

    /// Getters
    int getXStart() const { return m_xStart; }
    int getYStart() const { return m_yStart; }
    int getWidth()  const { return m_width; }
    int getHeight() const { return m_height; }

private:
    /// Coordinates and size of the area to refresh
    int m_xStart, m_yStart, m_width, m_height;
};


#endif
