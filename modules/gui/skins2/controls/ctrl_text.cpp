/*****************************************************************************
 * ctrl_text.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
 *          Erwan Tulou      <erwan10 At videolan Dot org<
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
                    uint32_t color, VarBool *pVisible, VarBool *pFocus,
                    Scrolling_t scrollMode, Align_t alignment ):
    CtrlGeneric( pIntf, rHelp, pVisible ), m_fsm( pIntf ),
    m_rVariable( rVariable ), m_cmdToManual( this ),
    m_cmdManualMoving( this ), m_cmdManualStill( this ),
    m_cmdMove( this ), m_pEvt( NULL ), m_rFont( rFont ),
    m_color( color ), m_scrollMode( scrollMode ), m_alignment( alignment ),
    m_pFocus( pFocus), m_pImg( NULL ), m_pImgDouble( NULL ),
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
    m_fsm.addTransition( "still", "leave", "outStill" );
    m_fsm.addTransition( "outStill", "enter", "still" );
    if( m_scrollMode == kManual )
    {
        m_fsm.addTransition( "still", "mouse:left:down", "manual1",
                             &m_cmdToManual );
        m_fsm.addTransition( "manual1", "mouse:left:up", "still",
                             &m_cmdManualStill );
        m_fsm.addTransition( "manual1", "motion", "manual1", &m_cmdMove );
    }
    else if( m_scrollMode == kAutomatic )
    {
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
        m_fsm.addTransition( "moving", "leave", "outMoving" );
        m_fsm.addTransition( "outMoving", "enter", "moving" );
    }

    // Initial state
    m_fsm.setState( (m_scrollMode != kAutomatic) ? "outStill" : "outMoving" );

    // Observe the variable
    m_rVariable.addObserver( this );

    // initialize pictures
    setPictures( m_rVariable.get() );
}


CtrlText::~CtrlText()
{
    m_rVariable.delObserver( this );
    delete m_pTimer;
    delete m_pImg;
    delete m_pImgDouble;
}


void CtrlText::handleEvent( EvtGeneric &rEvent )
{
    // Save the event to use it in callbacks
    m_pEvt = &rEvent;

    m_fsm.handleTransition( rEvent.getAsString() );
}


bool CtrlText::mouseOver( int x, int y ) const
{
    if( !m_pFocus->get() )
        return false;

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


void CtrlText::draw( OSGraphics &rImage, int xDest, int yDest, int w, int h )
{
    rect clip( xDest, yDest, w, h );
    const Position *pPos = getPosition();
    if( m_pCurrImg )
    {
        // Compute the dimensions to draw
        int width = min( m_pCurrImg->getWidth() + m_xPos,
                         getPosition()->getWidth() );
        int height = min( m_pCurrImg->getHeight(), getPosition()->getHeight() );
        // Draw the current image
        if( width > 0 && height > 0 )
        {
            int offset = 0;
            if( m_alignment == kLeft )
            {
                // We align to the left
                offset = 0;
            }
            else if( m_alignment == kRight &&
                     width < getPosition()->getWidth() )

            {
                // The text is shorter than the width of the control, so we
                // can align it to the right
                offset = getPosition()->getWidth() - width;
            }
            else if( m_alignment == kCenter &&
                     width < getPosition()->getWidth() )
            {
                // The text is shorter than the width of the control, so we
                // can center it
                offset = (getPosition()->getWidth() - width) / 2;
            }
            rect region( pPos->getLeft() + offset,
                         pPos->getTop(), width, height );
            rect inter;
            if( rect::intersect( region, clip, &inter ) )
                rImage.drawBitmap( *m_pCurrImg, -m_xPos + inter.x - region.x,
                                   inter.y - region.y,
                                   inter.x, inter.y,
                                   inter.width, inter.height, true );
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


void CtrlText::onUpdate( Subject<VarText> &rVariable, void* arg )
{
    (void)rVariable; (void)arg;
    if( isVisible() )
    {
        setPictures( m_rVariable.get() );
        updateContext();
        notifyLayout( getPosition()->getWidth(), getPosition()->getHeight() );
    }
}


void CtrlText::onUpdate( Subject<VarBool> &rVariable, void *arg  )
{
    (void)arg;
    // Visibility changed
    if( &rVariable == m_pVisible )
    {
        if( isVisible() )
        {
            setPictures( m_rVariable.get() );
            updateContext();
        }

        // notify in any case
        notifyLayout( getPosition()->getWidth(), getPosition()->getHeight() );
    }
}


void CtrlText::setPictures( const UString &rText )
{
    // reset the images ('normal' and 'double') from the text
    // 'Normal' image
    delete m_pImg;
    m_pImg = m_rFont.drawString( rText, m_color );
    if( !m_pImg )
        return;

    // 'Double' image
    const UString doubleStringWithSep = rText + SEPARATOR_STRING + rText;
    delete m_pImgDouble;
    m_pImgDouble = m_rFont.drawString( doubleStringWithSep, m_color );
}


void CtrlText::updateContext()
{
    if( !m_pImg || !getPosition() )
        return;

    if( m_pImg->getWidth() < getPosition()->getWidth() )
    {
        m_pCurrImg = m_pImg;

        // When the control becomes wide enough for the text to display,
        // make sure to stop any scrolling effect
        m_pTimer->stop();
        m_xPos = 0;
    }
    else
    {
        m_pCurrImg = m_pImgDouble;
    }

    // If the control is in the moving state,
    // automatically start or stop the timer accordingly
    const string &rState = m_fsm.getState();
    if( rState == "moving" || rState == "outMoving" )
    {
        if( m_pCurrImg == m_pImgDouble )
        {
            m_pTimer->start( MOVING_TEXT_DELAY, false );
        }
        else
        {
            m_pTimer->stop();
        }
    }

    // compute alignment
    if( m_alignment == kRight &&
        getPosition()->getWidth() < m_pImg->getWidth() )
    {
        m_xPos = getPosition()->getWidth() - m_pImg->getWidth();
    }
    else if( m_alignment == kCenter &&
             getPosition()->getWidth() < m_pImg->getWidth() )
    {
        m_xPos = (getPosition()->getWidth() - m_pImg->getWidth()) / 2;
    }
    else
    {
        m_xPos = 0;
    }
}


void CtrlText::onPositionChange()
{
    updateContext();
}


void CtrlText::onResize()
{
    updateContext();
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
    // control and if the control can scroll (either in manual or automatic
    // mode)
    if( m_pParent->m_pCurrImg &&
        m_pParent->m_pCurrImg == m_pParent->m_pImgDouble )
    {
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

    // Move text only when it is larger than the control
    if( m_pParent->m_pCurrImg &&
        m_pParent->m_pCurrImg == m_pParent->m_pImgDouble )
    {
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
    if( !m_pImg || !m_pImgDouble )
        return;

    // {m_pImgDouble->getWidth() - m_pImg->getWidth()} is the period of the
    // bitmap; remember that the string used to generate m_pImgDouble is of the
    // form: "foo  foo", the number of spaces being a parameter
    position %= m_pImgDouble->getWidth() - m_pImg->getWidth();
    if( position > 0 )
    {
        position -= m_pImgDouble->getWidth() - m_pImg->getWidth();
    }
}

