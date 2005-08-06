/*****************************************************************************
 * ctrl_move.cpp
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

#include "ctrl_move.hpp"
#include "../events/evt_generic.hpp"
#include "../events/evt_mouse.hpp"
#include "../events/evt_motion.hpp"
#include "../src/top_window.hpp"
#include "../src/window_manager.hpp"
#include "../utils/position.hpp"


CtrlMove::CtrlMove( intf_thread_t *pIntf, WindowManager &rWindowManager,
                    CtrlFlat &rCtrl, TopWindow &rWindow,
                    const UString &rHelp, VarBool *pVisible ):
    CtrlFlat( pIntf, rHelp, pVisible ), m_fsm( pIntf ),
    m_rWindowManager( rWindowManager ),
    m_rCtrl( rCtrl ), m_rWindow( rWindow ),
    m_cmdMovingMoving( pIntf, this ),
    m_cmdStillMoving( pIntf, this ),
    m_cmdMovingStill( pIntf, this )
{
    m_pEvt = NULL;
    m_xPos = 0;
    m_yPos = 0;

    // States
    m_fsm.addState( "moving" );
    m_fsm.addState( "still" );

    // Transitions
    m_fsm.addTransition( "moving", "mouse:left:up:none", "still",
                         &m_cmdMovingStill );
    m_fsm.addTransition( "still", "mouse:left:down:none", "moving",
                         &m_cmdStillMoving );
    m_fsm.addTransition( "moving", "motion", "moving", &m_cmdMovingMoving );

    m_fsm.setState( "still" );
}


bool CtrlMove::mouseOver( int x, int y ) const
{
    return m_rCtrl.mouseOver( x, y );
}


void CtrlMove::draw( OSGraphics &rImage, int xDest, int yDest )
{
    m_rCtrl.draw( rImage, xDest, yDest );
}


void CtrlMove::setLayout( GenericLayout *pLayout, const Position &rPosition )
{
    CtrlGeneric::setLayout( pLayout, rPosition );
    // Set the layout of the decorated control as well
    m_rCtrl.setLayout( pLayout, rPosition );
}


const Position *CtrlMove::getPosition() const
{
    return m_rCtrl.getPosition();
}


void CtrlMove::handleEvent( EvtGeneric &rEvent )
{
    m_pEvt = &rEvent;
    m_fsm.handleTransition( rEvent.getAsString() );
    // Transmit the event to the decorated control
    // XXX: Is it really a good idea?
    m_rCtrl.handleEvent( rEvent );
}


void CtrlMove::CmdStillMoving::execute()
{
    EvtMouse *pEvtMouse = (EvtMouse*)m_pControl->m_pEvt;

    m_pControl->m_xPos = pEvtMouse->getXPos();
    m_pControl->m_yPos = pEvtMouse->getYPos();

    m_pControl->captureMouse();

    m_pControl->m_rWindowManager.startMove( m_pControl->m_rWindow );
}


void CtrlMove::CmdMovingMoving::execute()
{
    EvtMotion *pEvtMotion = (EvtMotion*)m_pControl->m_pEvt;

    int xNewLeft = pEvtMotion->getXPos() - m_pControl->m_xPos +
                   m_pControl->m_rWindow.getLeft();
    int yNewTop = pEvtMotion->getYPos() - m_pControl->m_yPos +
                  m_pControl->m_rWindow.getTop();

    m_pControl->m_rWindowManager.move( m_pControl->m_rWindow, xNewLeft, yNewTop );
}


void CtrlMove::CmdMovingStill::execute()
{
    m_pControl->releaseMouse();

    m_pControl->m_rWindowManager.stopMove();
}
