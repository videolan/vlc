/*****************************************************************************
 * evt_mouse.hpp
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

#ifndef EVT_MOUSE_HPP
#define EVT_MOUSE_HPP

#include "evt_input.hpp"


/// Class for mouse button events
class EvtMouse: public EvtInput
{
public:
    enum ButtonType_t
    {
        kLeft,
        kMiddle,
        kRight
    };

    enum ActionType_t
    {
        kDown,
        kUp,
        kDblClick
    };

    EvtMouse( intf_thread_t *pIntf, int xPos, int yPos, ButtonType_t button,
              ActionType_t action, int mod = kModNone )
            : EvtInput( pIntf, mod ), m_xPos( xPos ), m_yPos( yPos ),
              m_button( button ), m_action( action ) { }
    virtual ~EvtMouse() { }
    virtual const std::string getAsString() const;

    // Return the event coordinates
    int getXPos() const { return m_xPos; }
    int getYPos() const { return m_yPos; }

    // Return the button and the action
    ButtonType_t getButton() const { return m_button; }
    ActionType_t getAction() const { return m_action; }

private:
    /// Coordinates of the mouse relative to the window
    int m_xPos, m_yPos;
    /// Mouse button involved in the event
    ButtonType_t m_button;
    /// Type of action
    ActionType_t m_action;
};


#endif
