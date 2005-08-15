/*****************************************************************************
 * ctrl_text.cpp
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

#include "ctrl_text.hpp"
#include "../events/evt_generic.hpp"
#include "../events/evt_mouse.hpp"
#include "../src/generic_bitmap.hpp"
#include "../src/generic_font.hpp"
#include "../src/os_factory.hpp"
#include "../src/os_graphics.hpp"
#include "../src/os_timer.hpp"
#include "../utils/position.hpp"
#include "../utils/ustring.hpp"
#include "../utils/var_text.hpp"


#define MOVING_TEXT_STEP 1
#define MOVING_TEXT_DELAY 30
#define SEPARATOR_STRING "   "


CtrlText::CtrlText( intf_thread_t *pIntf, VarText &rVariable,
                    const GenericFont &rFont, const UString &rHelp,
                    uint32_t color, VarBool *pVisible ):
    CtrlGeneric( pIntf, rHelp, pVisible ), m_fsm( pIntf ),
    m_rVariable( rVariable ), m_cmdToManual( this ),
    m_cmdManualMoving( this ), m_cmdManualStill( this ),
    m_cmdMove( this ), m_pEvt( NULL ), m_rFont( rFont ),
    m_color( color ), m_pImg( NULL ), m_pImgDouble( NULL ),
    m_pCurrImg( NULL ), m_xPos( 0 ), m_xOffset( 0 ),
    m_cmdUpdateText( this )
{
    m_pTimer = OSFactory::instance( pIntf )->createOSTimer( m_cmdUpdateText );

    // States
    m_fsm.addState( "still" );
    m_fsm.addState( "moving" );
    m_fsm.addState( "manual1" );
    m_fsm.addState( "manual2" );
    m_fsm.addState( "outStill" );
    m_fsm.addState( "outMoving" );

    // Transitions
    m_fsm.addTransition( "still", "mouse:left:down", "manual1",
                         &m_cmdToManual );
    m_fsm.addTransition( "manual1", "mouse:left:up", "moving",
                         &m_cmdManualMoving );
    m_fsm.addTransition( "moving", "mouse:left:down", "manual2",
                         &m_cmdToManual );
    m_fsm.addTransition( "manual2", "mouse:left:up", "still",
                         &m_cmdManualStill );
    m_fsm.addTransition( "manual1", "motion", "manual1", &m_cmdMove );
    m_fsm.addTransition( "manual2", "motion", "manual2", &m_cmdMove );
    m_fsm.addTransition( "still", "leave", "outStill" );
    m_fsm.addTransition( "outStill", "enter", "still" );
    m_fsm.addTransition( "moving", "leave", "outMoving" );
    m_fsm.addTransition( "outMoving", "enter", "moving" );

    // Initial state
    m_fsm.setState( "moving" );

    // Observe the variable
    m_rVariable.addObserver( this );

    // Set the text
    displayText( m_rVariable.get() );
}


CtrlText::~CtrlText()
{
    m_rVariable.delObserver( this );
    if( m_pTimer )
    {
        delete m_pTimer;
    }
    if( m_pImg )
    {
        delete m_pImg;
    }
    if( m_pImgDouble )
    {
        delete m_pImgDouble;
    }
}


void CtrlText::handleEvent( EvtGeneric &rEvent )
{
    // Save the event to use it in callbacks
    m_pEvt = &rEvent;

    m_fsm.handleTransition( rEvent.getAsString() );
}


bool CtrlText::mouseOver( int x, int y ) const
{
    if( m_pCurrImg )
    {
        // We have 3 different ways of deciding when to return true here:
        //  1) the mouse is exactly over the text (so if you click between two
        //     letters, the text control doesn't catch the event)
        //  2) the mouse is over the rectangle of the control
        //  3) the mouse is over the rectangle of the visible text
        // I don't know which one is the best...
#if 0
        return( x >= 0 && x < getPosition()->getWidth()
             && m_pCurrImg->hit( x - m_xPos, y ) );
#endif
#if 1
        return( x >= 0 && x < getPosition()->getWidth()
             && y >= 0 && y < getPosition()->getHeight() );
#endif
#if 0
        return( x >= 0 && x < getPosition()->getWidth()
             && y >= 0 && y < getPosition()->getHeight()
             && x < m_pCurrImg->getWidth() && x < m_pCurrImg->getHeight() );
#endif
    }
    else
    {
        return false;
    }
}


void CtrlText::draw( OSGraphics &rImage, int xDest, int yDest )
{
    if( m_pCurrImg )
    {
        // Compute the dimensions to draw
        int width = min( m_pCurrImg->getWidth() + m_xPos,
                         getPosition()->getWidth() );
        int height = min( m_pCurrImg->getHeight(), getPosition()->getHeight() );
        // Draw the current image
        if( width > 0 && height > 0 )
        {
            rImage.drawBitmap( *m_pCurrImg, -m_xPos, 0, xDest, yDest,
                            width, height, true );
        }
    }
}


void CtrlText::setText( const UString &rText, uint32_t color )
{
    // Change the color
    if( color != 0xFFFFFFFF )
    {
        m_color = color;
    }

    // Change the text
    m_rVariable.set( rText );
}


void CtrlText::onUpdate( Subject<VarText> &rVariable )
{
    displayText( m_rVariable.get() );
}


void CtrlText::displayText( const UString &rText )
{
    // Create the images ('normal' and 'double') from the text
    // 'Normal' image
    if( m_pImg )
    {
        delete m_pImg;
    }
    m_pImg = m_rFont.drawString( rText, m_color );
    if( !m_pImg )
    {
        return;
    }
    // 'Double' image
    const UString doubleStringWithSep = rText + SEPARATOR_STRING + rText;
    if( m_pImgDouble )
    {
        delete m_pImgDouble;
    }
    m_pImgDouble = m_rFont.drawString( doubleStringWithSep, m_color );

    // Update the current image used, as if the control size had changed
    onChangePosition();
    m_xPos = 0;

    if( getPosition() )
    {
        // If the control was in the moving state, check if the scrolling is
        // still necessary
        const string &rState = m_fsm.getState();
        if( rState == "moving" || rState == "outMoving" )
        {
            if( m_pImg && m_pImg->getWidth() >= getPosition()->getWidth() )
            {
                m_pCurrImg = m_pImgDouble;
                m_pTimer->start( MOVING_TEXT_DELAY, false );
            }
            else
            {
                m_pTimer->stop();
            }
        }
        notifyLayout( getPosition()->getWidth(), getPosition()->getHeight() );
    }
}


void CtrlText::onChangePosition()
{
    if( m_pImg && getPosition() )
    {
        if( m_pImg->getWidth() < getPosition()->getWidth() )
        {
            m_pCurrImg = m_pImg;
        }
        else
        {
            m_pCurrImg = m_pImgDouble;
        }
    }
    else
    {
        // m_pImg is a better default value than m_pImgDouble, but anyway we
        // don't care because the control is never drawn without position :)
        m_pCurrImg = m_pImg;
    }
}


void CtrlText::CmdToManual::execute()
{
    EvtMouse *pEvtMouse = (EvtMouse*)m_pParent->m_pEvt;

    // Compute the offset
    m_pParent->m_xOffset = pEvtMouse->getXPos() - m_pParent->m_xPos;

    m_pParent->m_pTimer->stop();
    m_pParent->captureMouse();
}


void CtrlText::CmdManualMoving::execute()
{
    m_pParent->releaseMouse();

    // Start the automatic movement, but only if the text is wider than the
    // control
    if( m_pParent->m_pImg &&
        m_pParent->m_pImg->getWidth() >= m_pParent->getPosition()->getWidth() )
    {
        // The current image may have been set incorrectly in displayText(), so
        // set the correct value
        m_pParent->m_pCurrImg = m_pParent->m_pImgDouble;

        m_pParent->m_pTimer->start( MOVING_TEXT_DELAY, false );
    }
}


void CtrlText::CmdManualStill::execute()
{
    m_pParent->releaseMouse();
}


void CtrlText::CmdMove::execute()
{
    EvtMouse *pEvtMouse = (EvtMouse*)m_pParent->m_pEvt;

    // Do nothing if the text fits in the control
    if( m_pParent->m_pImg &&
        m_pParent->m_pImg->getWidth() >= m_pParent->getPosition()->getWidth() )
    {
        // The current image may have been set incorrectly in displayText(), so
        // we set the correct value
        m_pParent->m_pCurrImg = m_pParent->m_pImgDouble;

        // Compute the new position of the left side, and make sure it is
        // in the correct range
        m_pParent->m_xPos = (pEvtMouse->getXPos() - m_pParent->m_xOffset);
        m_pParent->adjust( m_pParent->m_xPos );

        m_pParent->notifyLayout( m_pParent->getPosition()->getWidth(),
                             m_pParent->getPosition()->getHeight() );
    }
}


void CtrlText::CmdUpdateText::execute()
{
    m_pParent->m_xPos -= MOVING_TEXT_STEP;
    m_pParent->adjust( m_pParent->m_xPos );

    m_pParent->notifyLayout( m_pParent->getPosition()->getWidth(),
                         m_pParent->getPosition()->getHeight() );
}


void CtrlText::adjust( int &position )
{
    // {m_pImgDouble->getWidth()  - m_pImg->getWidth()} is the period of the
    // bitmap; remember that the string used to generate m_pImgDouble is of the
    // form: "foo  foo", the number of spaces being a parameter
    if( !m_pImg )
    {
        return;
    }
    position %= m_pImgDouble->getWidth() - m_pImg->getWidth();
    if( position > 0 )
    {
        position -= m_pImgDouble->getWidth() - m_pImg->getWidth();
    }
}

