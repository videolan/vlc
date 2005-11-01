/*****************************************************************************
 * ctrl_resize.cpp
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

#include "ctrl_resize.hpp"
#include "../events/evt_generic.hpp"
#include "../events/evt_mouse.hpp"
#include "../events/evt_motion.hpp"
#include "../src/generic_layout.hpp"
#include "../src/os_factory.hpp"
#include "../utils/position.hpp"
#include "../commands/async_queue.hpp"
#include "../commands/cmd_resize.hpp"


CtrlResize::CtrlResize( intf_thread_t *pIntf, CtrlFlat &rCtrl,
                        GenericLayout &rLayout, const UString &rHelp,
                        VarBool *pVisible ):
    CtrlFlat( pIntf, rHelp, pVisible ), m_fsm( pIntf ), m_rCtrl( rCtrl ),
    m_rLayout( rLayout ), m_cmdOutStill( this ),
    m_cmdStillOut( this ),
    m_cmdStillStill( this ),
    m_cmdStillResize( this ),
    m_cmdResizeStill( this ),
    m_cmdResizeResize( this )
{
    m_pEvt = NULL;
    m_xPos = 0;
    m_yPos = 0;

    // States
    m_fsm.addState( "out" );
    m_fsm.addState( "still" );
    m_fsm.addState( "resize" );

    // Transitions
    m_fsm.addTransition( "out", "enter", "still", &m_cmdOutStill );
    m_fsm.addTransition( "still", "leave", "out", &m_cmdStillOut );
    m_fsm.addTransition( "still", "motion", "still", &m_cmdStillStill );
    m_fsm.addTransition( "resize", "mouse:left:up:none", "still",
                         &m_cmdResizeStill );
    m_fsm.addTransition( "still", "mouse:left:down:none", "resize",
                         &m_cmdStillResize );
    m_fsm.addTransition( "resize", "motion", "resize", &m_cmdResizeResize );

    m_fsm.setState( "still" );
}


bool CtrlResize::mouseOver( int x, int y ) const
{
    return m_rCtrl.mouseOver( x, y );
}


void CtrlResize::draw( OSGraphics &rImage, int xDest, int yDest )
{
    m_rCtrl.draw( rImage, xDest, yDest );
}


void CtrlResize::setLayout( GenericLayout *pLayout, const Position &rPosition )
{
    CtrlGeneric::setLayout( pLayout, rPosition );
    // Set the layout of the decorated control as well
    m_rCtrl.setLayout( pLayout, rPosition );
}


const Position *CtrlResize::getPosition() const
{
    return m_rCtrl.getPosition();
}


void CtrlResize::handleEvent( EvtGeneric &rEvent )
{
    m_pEvt = &rEvent;
    m_fsm.handleTransition( rEvent.getAsString() );
    // Transmit the event to the decorated control
    // XXX: Is it really a good idea?
    m_rCtrl.handleEvent( rEvent );
}


void CtrlResize::CmdOutStill::execute()
{
    OSFactory *pOsFactory = OSFactory::instance( m_pParent->getIntf() );
    pOsFactory->changeCursor( OSFactory::kResizeNWSE );
}


void CtrlResize::CmdStillOut::execute()
{
    OSFactory *pOsFactory = OSFactory::instance( m_pParent->getIntf() );
    pOsFactory->changeCursor( OSFactory::kDefaultArrow );
}


void CtrlResize::CmdStillStill::execute()
{
    OSFactory *pOsFactory = OSFactory::instance( m_pParent->getIntf() );
    pOsFactory->changeCursor( OSFactory::kResizeNWSE );
}


void CtrlResize::CmdStillResize::execute()
{
    EvtMouse *pEvtMouse = (EvtMouse*)m_pParent->m_pEvt;

    // Set the cursor
    OSFactory *pOsFactory = OSFactory::instance( m_pParent->getIntf() );
    pOsFactory->changeCursor( OSFactory::kResizeNWSE );

    m_pParent->m_xPos = pEvtMouse->getXPos();
    m_pParent->m_yPos = pEvtMouse->getYPos();

    m_pParent->captureMouse();

    m_pParent->m_width = m_pParent->m_rLayout.getWidth();
    m_pParent->m_height = m_pParent->m_rLayout.getHeight();
}


void CtrlResize::CmdResizeStill::execute()
{
    // Set the cursor
    OSFactory *pOsFactory = OSFactory::instance( m_pParent->getIntf() );
    pOsFactory->changeCursor( OSFactory::kResizeNWSE );

    m_pParent->releaseMouse();
}


void CtrlResize::CmdResizeResize::execute()
{
    EvtMotion *pEvtMotion = (EvtMotion*)m_pParent->m_pEvt;

    // Set the cursor
    OSFactory *pOsFactory = OSFactory::instance( m_pParent->getIntf() );
    pOsFactory->changeCursor( OSFactory::kResizeNWSE );

    int newWidth = pEvtMotion->getXPos() - m_pParent->m_xPos + m_pParent->m_width;
    int newHeight = pEvtMotion->getYPos() - m_pParent->m_yPos + m_pParent->m_height;

    // Create a resize command
    CmdGeneric *pCmd = new CmdResize( m_pParent->getIntf(), m_pParent->m_rLayout,
                                      newWidth, newHeight );
    // Push the command in the asynchronous command queue
    AsyncQueue *pQueue = AsyncQueue::instance( m_pParent->getIntf() );
    pQueue->remove( "resize" );
    pQueue->push( CmdGenericPtr( pCmd ) );
}
