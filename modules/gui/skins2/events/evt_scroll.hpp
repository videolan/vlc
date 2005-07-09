/*****************************************************************************
 * evt_scroll.hpp
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef EVT_SCROLL_HPP
#define EVT_SCROLL_HPP

#include "evt_input.hpp"


/// Class for mouse scroll events
class EvtScroll: public EvtInput
{
    public:
        typedef enum
        {
            kUp,
            kDown
        } Direction_t;

        EvtScroll( intf_thread_t *pIntf, int xPos, int yPos,
                   Direction_t direction, int mod = kModNone ):
            EvtInput( pIntf, mod ), m_xPos( xPos ), m_yPos( yPos ),
            m_direction( direction ) {}
        virtual ~EvtScroll() {}

        // Return the event coordinates
        int getXPos() const { return m_xPos; }
        int getYPos() const { return m_yPos; }

        // Return the direction
        Direction_t getDirection() const { return m_direction; }

        virtual const string getAsString() const;

    private:
        /// Coordinates of the mouse relative to the window
        int m_xPos, m_yPos;
        /// Scroll direction
        Direction_t m_direction;
};


#endif
