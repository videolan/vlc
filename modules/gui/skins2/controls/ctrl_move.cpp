/*****************************************************************************
 * ctrl_move.cpp
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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
    m_cmdMovingMoving( this ),
    m_cmdStillMoving( this ),
    m_cmdMovingStill( this )
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


void CtrlMove::draw( OSGraphics &rImage, int xDest, int yDest, int w, int h )
{
    m_rCtrl.draw( rImage, xDest, yDest, w, h );
}


void CtrlMove::setLayout( GenericLayout *pLayout, const Position &rPosition )
{
    CtrlGeneric::setLayout( pLayout, rPosition );
    // Set the layout of the decorated control as well
    m_rCtrl.setLayout( pLayout, rPosition );
}


void CtrlMove::unsetLayout( )
{
    m_rCtrl.unsetLayout();
    CtrlGeneric::unsetLayout();
}


const Position *CtrlMove::getPosition() const
{
    return m_rCtrl.getPosition();
}


void CtrlMove::onResize()
{
    m_rCtrl.onResize();
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
    EvtMouse *pEvtMouse = (EvtMouse*)m_pParent->m_pEvt;

    m_pParent->m_xPos = pEvtMouse->getXPos();
    m_pParent->m_yPos = pEvtMouse->getYPos();

    m_pParent->captureMouse();

    m_pParent->m_rWindowManager.startMove( m_pParent->m_rWindow );
}


void CtrlMove::CmdMovingMoving::execute()
{
    EvtMotion *pEvtMotion = (EvtMotion*)m_pParent->m_pEvt;

    int xNewLeft = pEvtMotion->getXPos() - m_pParent->m_xPos +
                   m_pParent->m_rWindow.getLeft();
    int yNewTop = pEvtMotion->getYPos() - m_pParent->m_yPos +
                  m_pParent->m_rWindow.getTop();

    m_pParent->m_rWindowManager.move( m_pParent->m_rWindow, xNewLeft, yNewTop );
}


void CtrlMove::CmdMovingStill::execute()
{
    m_pParent->releaseMouse();

    m_pParent->m_rWindowManager.stopMove();
}
