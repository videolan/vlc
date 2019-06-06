/*****************************************************************************
 * ctrl_resize.cpp
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

#include "ctrl_resize.hpp"
#include "../events/evt_generic.hpp"
#include "../events/evt_mouse.hpp"
#include "../events/evt_motion.hpp"
#include "../src/generic_layout.hpp"
#include "../src/os_factory.hpp"
#include "../utils/position.hpp"
#include "../commands/async_queue.hpp"
#include "../commands/cmd_resize.hpp"


CtrlResize::CtrlResize( intf_thread_t *pIntf, WindowManager &rWindowManager,
                        CtrlFlat &rCtrl, GenericLayout &rLayout,
                        const UString &rHelp, VarBool *pVisible,
                        WindowManager::Direction_t direction ):
    CtrlFlat( pIntf, rHelp, pVisible ), m_fsm( pIntf ),
    m_rWindowManager( rWindowManager ), m_rCtrl( rCtrl ),
    m_rLayout( rLayout ), m_direction( direction ),  m_cmdOutStill( this ),
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


void CtrlResize::draw( OSGraphics &rImage, int xDest, int yDest, int w, int h )
{
    m_rCtrl.draw( rImage, xDest, yDest, w, h );
}


void CtrlResize::setLayout( GenericLayout *pLayout, const Position &rPosition )
{
    CtrlGeneric::setLayout( pLayout, rPosition );
    // Set the layout of the decorated control as well
    m_rCtrl.setLayout( pLayout, rPosition );
}


void CtrlResize::unsetLayout()
{
    m_rCtrl.unsetLayout();
    CtrlGeneric::unsetLayout();
}


const Position *CtrlResize::getPosition() const
{
    return m_rCtrl.getPosition();
}


void CtrlResize::onResize()
{
    m_rCtrl.onResize();
}


void CtrlResize::handleEvent( EvtGeneric &rEvent )
{
    m_pEvt = &rEvent;
    m_fsm.handleTransition( rEvent.getAsString() );
    // Transmit the event to the decorated control
    // XXX: Is it really a good idea?
    m_rCtrl.handleEvent( rEvent );
}


void CtrlResize::changeCursor( WindowManager::Direction_t direction ) const
{
    OSFactory::CursorType_t cursor;
    switch( direction )
    {
    default:
    case WindowManager::kNone:     cursor = OSFactory::kDefaultArrow; break;
    case WindowManager::kResizeSE: cursor = OSFactory::kResizeNWSE;   break;
    case WindowManager::kResizeS:  cursor = OSFactory::kResizeNS;     break;
    case WindowManager::kResizeE:  cursor = OSFactory::kResizeWE;     break;
    }
    OSFactory::instance( getIntf() )->changeCursor( cursor );
}


void CtrlResize::CmdOutStill::execute()
{
    m_pParent->changeCursor( m_pParent->m_direction );
}


void CtrlResize::CmdStillOut::execute()
{
    m_pParent->changeCursor( WindowManager::kNone );
}


void CtrlResize::CmdStillStill::execute()
{
    m_pParent->changeCursor( m_pParent->m_direction );
}


void CtrlResize::CmdStillResize::execute()
{
    EvtMouse *pEvtMouse = static_cast<EvtMouse*>(m_pParent->m_pEvt);

    // Set the cursor
    m_pParent->changeCursor( m_pParent->m_direction );

    m_pParent->m_xPos = pEvtMouse->getXPos();
    m_pParent->m_yPos = pEvtMouse->getYPos();

    m_pParent->captureMouse();

    m_pParent->m_width = m_pParent->m_rLayout.getWidth();
    m_pParent->m_height = m_pParent->m_rLayout.getHeight();

    m_pParent->m_rWindowManager.startResize( m_pParent->m_rLayout,
                                             m_pParent->m_direction);
}


void CtrlResize::CmdResizeStill::execute()
{
    // Set the cursor
    m_pParent->changeCursor( WindowManager::kNone );

    m_pParent->releaseMouse();

    m_pParent->m_rWindowManager.stopResize();
}


void CtrlResize::CmdResizeResize::execute()
{
    EvtMotion *pEvtMotion = static_cast<EvtMotion*>(m_pParent->m_pEvt);

    m_pParent->changeCursor( m_pParent->m_direction );

    int newWidth = m_pParent->m_width;
    newWidth += pEvtMotion->getXPos() - m_pParent->m_xPos;
    int newHeight = m_pParent->m_height;
    newHeight += pEvtMotion->getYPos() - m_pParent->m_yPos;

    // Create a resize command, instead of calling the window manager directly.
    // Thanks to this trick, the duplicate resizing commands will be trashed
    // in the asynchronous queue, thus making resizing faster
    CmdGeneric *pCmd = new CmdResize( m_pParent->getIntf(),
                                      m_pParent->m_rWindowManager,
                                      m_pParent->m_rLayout,
                                      newWidth, newHeight );
    // Push the command in the asynchronous command queue
    AsyncQueue::instance( getIntf() )->push( CmdGenericPtr( pCmd ) );
}
