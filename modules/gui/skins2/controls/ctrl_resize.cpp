/*****************************************************************************
 * ctrl_resize.cpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN (Centrale RÃ©seaux) and its contributors
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
    m_rLayout( rLayout ), m_cmdOutStill( this, &transOutStill ),
    m_cmdStillOut( this, &transStillOut ),
    m_cmdStillStill( this, &transStillStill ),
    m_cmdStillResize( this, &transStillResize ),
    m_cmdResizeStill( this, &transResizeStill ),
    m_cmdResizeResize( this, &transResizeResize )
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


void CtrlResize::transOutStill( SkinObject *pCtrl )
{
    CtrlResize *pThis = (CtrlResize*)pCtrl;
    OSFactory *pOsFactory = OSFactory::instance( pThis->getIntf() );
    pOsFactory->changeCursor( OSFactory::kResizeNWSE );
}


void CtrlResize::transStillOut( SkinObject *pCtrl )
{
    CtrlResize *pThis = (CtrlResize*)pCtrl;
    OSFactory *pOsFactory = OSFactory::instance( pThis->getIntf() );
    pOsFactory->changeCursor( OSFactory::kDefaultArrow );
}


void CtrlResize::transStillStill( SkinObject *pCtrl )
{
    CtrlResize *pThis = (CtrlResize*)pCtrl;
    OSFactory *pOsFactory = OSFactory::instance( pThis->getIntf() );
    pOsFactory->changeCursor( OSFactory::kResizeNWSE );
}


void CtrlResize::transStillResize( SkinObject *pCtrl )
{
    CtrlResize *pThis = (CtrlResize*)pCtrl;
    EvtMouse *pEvtMouse = (EvtMouse*)pThis->m_pEvt;

    // Set the cursor
    OSFactory *pOsFactory = OSFactory::instance( pThis->getIntf() );
    pOsFactory->changeCursor( OSFactory::kResizeNWSE );

    pThis->m_xPos = pEvtMouse->getXPos();
    pThis->m_yPos = pEvtMouse->getYPos();

    pThis->captureMouse();

    pThis->m_width = pThis->m_rLayout.getWidth();
    pThis->m_height = pThis->m_rLayout.getHeight();
}


void CtrlResize::transResizeStill( SkinObject *pCtrl )
{
    CtrlResize *pThis = (CtrlResize*)pCtrl;

    // Set the cursor
    OSFactory *pOsFactory = OSFactory::instance( pThis->getIntf() );
    pOsFactory->changeCursor( OSFactory::kResizeNWSE );

    pThis->releaseMouse();
}


void CtrlResize::transResizeResize( SkinObject *pCtrl )
{
    CtrlResize *pThis = (CtrlResize*)pCtrl;
    EvtMotion *pEvtMotion = (EvtMotion*)pThis->m_pEvt;

    // Set the cursor
    OSFactory *pOsFactory = OSFactory::instance( pThis->getIntf() );
    pOsFactory->changeCursor( OSFactory::kResizeNWSE );

    int newWidth = pEvtMotion->getXPos() - pThis->m_xPos + pThis->m_width;
    int newHeight = pEvtMotion->getYPos() - pThis->m_yPos + pThis->m_height;

    // Check boundaries
    if( newWidth < pThis->m_rLayout.getMinWidth() )
    {
        newWidth = pThis->m_rLayout.getMinWidth();
    }
    if( newWidth > pThis->m_rLayout.getMaxWidth() )
    {
        newWidth = pThis->m_rLayout.getMaxWidth();
    }
    if( newHeight < pThis->m_rLayout.getMinHeight() )
    {
        newHeight = pThis->m_rLayout.getMinHeight();
    }
    if( newHeight > pThis->m_rLayout.getMaxHeight() )
    {
        newHeight = pThis->m_rLayout.getMaxHeight();
    }

    // Create a resize command
    CmdGeneric *pCmd = new CmdResize( pThis->getIntf(), pThis->m_rLayout,
                                      newWidth, newHeight );
    // Push the command in the asynchronous command queue
    AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
    pQueue->remove( "resize" );
    pQueue->push( CmdGenericPtr( pCmd ) );
}
