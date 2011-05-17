/*****************************************************************************
 * ctrl_button.cpp
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "ctrl_button.hpp"
#include "../events/evt_generic.hpp"
#include "../src/generic_bitmap.hpp"
#include "../src/generic_layout.hpp"
#include "../src/os_factory.hpp"
#include "../src/os_graphics.hpp"
#include "../commands/cmd_generic.hpp"


CtrlButton::CtrlButton( intf_thread_t *pIntf, const GenericBitmap &rBmpUp,
                        const GenericBitmap &rBmpOver,
                        const GenericBitmap &rBmpDown, CmdGeneric &rCommand,
                        const UString &rTooltip, const UString &rHelp,
                        VarBool *pVisible ):
    CtrlGeneric( pIntf, rHelp, pVisible ), m_fsm( pIntf ),
    m_rCommand( rCommand ), m_tooltip( rTooltip ),
    m_imgUp( pIntf, rBmpUp ), m_imgOver( pIntf, rBmpOver ),
    m_imgDown( pIntf, rBmpDown ), m_pImg( NULL ), m_cmdUpOverDownOver( this ),
    m_cmdDownOverUpOver( this ), m_cmdDownOverDown( this ),
    m_cmdDownDownOver( this ), m_cmdUpOverUp( this ), m_cmdUpUpOver( this ),
    m_cmdDownUp( this ), m_cmdUpHidden( this ), m_cmdHiddenUp( this )
{
    // States
    m_fsm.addState( "up" );
    m_fsm.addState( "down" );
    m_fsm.addState( "upOver" );
    m_fsm.addState( "downOver" );
    m_fsm.addState( "hidden" );

    // Transitions
    m_fsm.addTransition( "upOver", "mouse:left:down", "downOver",
                         &m_cmdUpOverDownOver );
    m_fsm.addTransition( "upOver", "mouse:left:dblclick", "downOver",
                         &m_cmdUpOverDownOver );
    m_fsm.addTransition( "downOver", "mouse:left:up", "upOver",
                         &m_cmdDownOverUpOver );
    m_fsm.addTransition( "downOver", "leave", "down", &m_cmdDownOverDown );
    m_fsm.addTransition( "down", "enter", "downOver", &m_cmdDownDownOver );
    m_fsm.addTransition( "upOver", "leave", "up", &m_cmdUpOverUp );
    m_fsm.addTransition( "up", "enter", "upOver", &m_cmdUpUpOver );
    m_fsm.addTransition( "down", "mouse:left:up", "up", &m_cmdDownUp );
    // XXX: It would be easy to use a "ANY" initial state to handle these
    // four lines in only one. But till now it isn't worthwhile...
    m_fsm.addTransition( "up", "special:hide", "hidden", &m_cmdUpHidden );
    m_fsm.addTransition( "down", "special:hide", "hidden", &m_cmdUpHidden );
    m_fsm.addTransition( "upOver", "special:hide", "hidden", &m_cmdUpHidden );
    m_fsm.addTransition( "downOver", "special:hide", "hidden", &m_cmdUpHidden );
    m_fsm.addTransition( "hidden", "special:show", "up", &m_cmdHiddenUp );

    // Initial state
    m_fsm.setState( "up" );
    setImage( &m_imgUp );
}


CtrlButton::~CtrlButton()
{
    if( m_pImg )
    {
        m_pImg->stopAnim();
        m_pImg->delObserver( this );
    }
}

void CtrlButton::setLayout( GenericLayout *pLayout,
                           const Position &rPosition )
{
    CtrlGeneric::setLayout( pLayout, rPosition );
    m_pLayout->getActiveVar().addObserver( this );
}


void CtrlButton::unsetLayout()
{
    m_pLayout->getActiveVar().delObserver( this );
    CtrlGeneric::unsetLayout();
}

void CtrlButton::handleEvent( EvtGeneric &rEvent )
{
    m_fsm.handleTransition( rEvent.getAsString() );
}


bool CtrlButton::mouseOver( int x, int y ) const
{
    if( m_pImg )
    {
        return m_pImg->hit( x, y );
    }
    else
    {
        return false;
    }
}


void CtrlButton::draw( OSGraphics &rImage, int xDest, int yDest, int w, int h )
{
    const Position *pPos = getPosition();
    rect region( pPos->getLeft(), pPos->getTop(),
                 pPos->getWidth(), pPos->getHeight() );
    rect clip( xDest, yDest, w, h );
    rect inter;
    if( rect::intersect( region, clip, &inter ) && m_pImg )
    {
        // Draw the current image
        m_pImg->draw( rImage, inter.x, inter.y, inter.width, inter.height,
                      inter.x - pPos->getLeft(),
                      inter.y - pPos->getTop() );
    }
}

void CtrlButton::setImage( AnimBitmap *pImg )
{
    if( pImg == m_pImg )
        return;

    AnimBitmap *pOldImg = m_pImg;
    m_pImg = pImg;

    if( pOldImg )
    {
        pOldImg->stopAnim();
        pOldImg->delObserver( this );
    }

    if( pImg )
    {
        pImg->startAnim();
        pImg->addObserver( this );
    }

    notifyLayoutMaxSize( pOldImg, pImg );
}


void CtrlButton::onUpdate( Subject<AnimBitmap> &rBitmap, void *arg )
{
    (void)rBitmap;(void)arg;
    notifyLayout( m_pImg->getWidth(), m_pImg->getHeight() );
}


void CtrlButton::CmdUpOverDownOver::execute()
{
    m_pParent->captureMouse();
    m_pParent->setImage( &m_pParent->m_imgDown );
}


void CtrlButton::CmdDownOverUpOver::execute()
{
    m_pParent->releaseMouse();
    m_pParent->setImage( &m_pParent->m_imgUp );
    // Execute the command associated to this button
    m_pParent->m_rCommand.execute();
}


void CtrlButton::CmdDownOverDown::execute()
{
    m_pParent->setImage( &m_pParent->m_imgUp );
}


void CtrlButton::CmdDownDownOver::execute()
{
    m_pParent->setImage( &m_pParent->m_imgDown );
}


void CtrlButton::CmdUpUpOver::execute()
{
    m_pParent->setImage( &m_pParent->m_imgOver );
}


void CtrlButton::CmdUpOverUp::execute()
{
    m_pParent->setImage( &m_pParent->m_imgUp );
}


void CtrlButton::CmdDownUp::execute()
{
    m_pParent->releaseMouse();
}


void CtrlButton::CmdUpHidden::execute()
{
    m_pParent->setImage( NULL );
}


void CtrlButton::CmdHiddenUp::execute()
{
    m_pParent->setImage( &m_pParent->m_imgUp );
}

void CtrlButton::onUpdate( Subject<VarBool> &rVariable, void *arg  )
{
    // restart animation
    if(     &rVariable == m_pVisible
        ||  &rVariable == &m_pLayout->getActiveVar()
      )
    {
        if( m_pImg )
        {
            m_pImg->stopAnim();
            m_pImg->startAnim();
        }
    }
    CtrlGeneric::onUpdate( rVariable, arg );
}

