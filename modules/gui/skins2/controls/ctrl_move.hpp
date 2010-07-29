/*****************************************************************************
 * ctrl_move.hpp
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

#ifndef CTRL_MOVE_HPP
#define CTRL_MOVE_HPP

#include "../commands/cmd_generic.hpp"
#include "../utils/fsm.hpp"
#include "ctrl_flat.hpp"

class TopWindow;
class WindowManager;


/// Control for moving windows
class CtrlMove: public CtrlFlat
{
public:
    CtrlMove( intf_thread_t *pIntf, WindowManager &rWindowManager,
              CtrlFlat &rCtrl, TopWindow &rWindow,
              const UString &rHelp, VarBool *pVisible );
    virtual ~CtrlMove() { }

    /// Handle an event
    virtual void handleEvent( EvtGeneric &rEvent );

    /// Check whether coordinates are inside the decorated control
    virtual bool mouseOver( int x, int y ) const;

    /// Draw the control on the given graphics
    virtual void draw( OSGraphics &rImage, int xDest, int yDest, int w, int h );

    /// Set the position and the associated layout of the decorated control
    virtual void setLayout( GenericLayout *pLayout,
                            const Position &rPosition );
    virtual void unsetLayout( );

    /// Get the position of the decorated control in the layout, if any
    virtual const Position *getPosition() const;

    /// Method called when the control is resized
    virtual void onResize();

    /// Get the type of control (custom RTTI)
    virtual string getType() const { return m_rCtrl.getType(); }

private:
    FSM m_fsm;
    /// Window manager
    WindowManager &m_rWindowManager;
    /// Decorated CtrlFlat
    CtrlFlat &m_rCtrl;
    /// The window moved by this control
    TopWindow &m_rWindow;
    /// The last received event
    EvtGeneric *m_pEvt;
    /// Position of the click that started the move
    int m_xPos, m_yPos;

    /// Callback objects
    DEFINE_CALLBACK( CtrlMove, MovingMoving )
    DEFINE_CALLBACK( CtrlMove, StillMoving )
    DEFINE_CALLBACK( CtrlMove, MovingStill )
};

#endif
