/*****************************************************************************
 * evt_motion.hpp
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

#ifndef EVT_MOTION_HPP
#define EVT_MOTION_HPP

#include "evt_input.hpp"


/// Mouse move event
class EvtMotion: public EvtInput
{
public:
    EvtMotion( intf_thread_t *pIntf, int xPos, int yPos )
             : EvtInput( pIntf ), m_xPos( xPos ), m_yPos( yPos ) { }
    virtual ~EvtMotion() { }
    virtual const std::string getAsString() const { return "motion"; }

    // Getters
    int getXPos() const { return m_xPos; }
    int getYPos() const { return m_yPos; }

private:
    /// Coordinates of the mouse (absolute or relative)
    /**
     * The coordinates are absolute when the event is sent to the
     * GenericWindow, but are relative to the window when the event is
     * forwarded to the controls
     */
    int m_xPos, m_yPos;
};


#endif
