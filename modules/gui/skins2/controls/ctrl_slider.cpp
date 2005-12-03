/*****************************************************************************
 * ctrl_slider.cpp
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

#include "ctrl_slider.hpp"
#include "../events/evt_enter.hpp"
#include "../events/evt_mouse.hpp"
#include "../events/evt_scroll.hpp"
#include "../src/generic_bitmap.hpp"
#include "../src/top_window.hpp"
#include "../src/os_factory.hpp"
#include "../src/os_graphics.hpp"
#include "../utils/position.hpp"
#include "../utils/var_percent.hpp"


#define RANGE 40
#define SCROLL_STEP 0.05f


CtrlSliderCursor::CtrlSliderCursor( intf_thread_t *pIntf,
                                    const GenericBitmap &rBmpUp,
                                    const GenericBitmap &rBmpOver,
                                    const GenericBitmap &rBmpDown,
                                    const Bezier &rCurve,
                                    VarPercent &rVariable,
                                    VarBool *pVisible,
                                    const UString &rTooltip,
                                    const UString &rHelp ):
    CtrlGeneric( pIntf, rHelp, pVisible ), m_fsm( pIntf ),
    m_rVariable( rVariable ), m_tooltip( rTooltip ),
    m_width( rCurve.getWidth() ), m_height( rCurve.getHeight() ),
    m_cmdOverDown( this ), m_cmdDownOver( this ),
    m_cmdOverUp( this ), m_cmdUpOver( this ),
    m_cmdMove( this ), m_cmdScroll( this ),
    m_lastPercentage( 0 ), m_xOffset( 0 ), m_yOffset( 0 ),
    m_pEvt( NULL ), m_rCurve( rCurve )
{
    // Build the images of the cursor
    OSFactory *pOsFactory = OSFactory::instance( getIntf() );
    m_pImgUp = pOsFactory->createOSGraphics( rBmpUp.getWidth(),
                                             rBmpUp.getHeight() );
    m_pImgUp->drawBitmap( rBmpUp, 0, 0 );
    m_pImgDown = pOsFactory->createOSGraphics( rBmpDown.getWidth(),
                                               rBmpDown.getHeight() );
    m_pImgDown->drawBitmap( rBmpDown, 0, 0 );
    m_pImgOver = pOsFactory->createOSGraphics( rBmpOver.getWidth(),
                                               rBmpOver.getHeight() );
    m_pImgOver->drawBitmap( rBmpOver, 0, 0 );

    // States
    m_fsm.addState( "up" );
    m_fsm.addState( "over" );
    m_fsm.addState( "down" );

    // Transitions
    m_fsm.addTransition( "over", "mouse:left:down", "down",
                         &m_cmdOverDown );
    m_fsm.addTransition( "down", "mouse:left:up", "over",
                         &m_cmdDownOver );
    m_fsm.addTransition( "over", "leave", "up", &m_cmdOverUp );
    m_fsm.addTransition( "up", "enter", "over", &m_cmdUpOver );
    m_fsm.addTransition( "down", "motion", "down", &m_cmdMove );
    m_fsm.addTransition( "over", "scroll", "over", &m_cmdScroll );

    // Initial state
    m_fsm.setState( "up" );
    m_pImg = m_pImgUp;

    // Observe the position variable
    m_rVariable.addObserver( this );

    // Initial position of the cursor
    m_lastPercentage = m_rVariable.get();
}


CtrlSliderCursor::~CtrlSliderCursor()
{
    m_rVariable.delObserver( this );
    SKINS_DELETE( m_pImgUp );
    SKINS_DELETE( m_pImgDown );
    SKINS_DELETE( m_pImgOver );
}


void CtrlSliderCursor::handleEvent( EvtGeneric &rEvent )
{
    // Save the event to use it in callbacks
    m_pEvt = &rEvent;

    m_fsm.handleTransition( rEvent.getAsString() );
}


bool CtrlSliderCursor::mouseOver( int x, int y ) const
{
    if( m_pImg )
    {
        // Compute the position of the cursor
        int xPos, yPos;
        m_rCurve.getPoint( m_rVariable.get(), xPos, yPos );

        // Compute the resize factors
        float factorX, factorY;
        getResizeFactors( factorX, factorY );
        xPos = (int)(xPos * factorX);
        yPos = (int)(yPos * factorY);

        return m_pImg->hit( x - xPos + m_pImg->getWidth() / 2,
                            y - yPos + m_pImg->getHeight() / 2 );
    }
    else
    {
        return false;
    }
}


void CtrlSliderCursor::draw( OSGraphics &rImage, int xDest, int yDest )
{
    if( m_pImg )
    {
        // Compute the position of the cursor
        int xPos, yPos;
        m_rCurve.getPoint( m_rVariable.get(), xPos, yPos );

        // Compute the resize factors
        float factorX, factorY;
        getResizeFactors( factorX, factorY );
        xPos = (int)(xPos * factorX);
        yPos = (int)(yPos * factorY);

        // Draw the current image
        rImage.drawGraphics( *m_pImg, 0, 0,
                             xDest + xPos - m_pImg->getWidth() / 2,
                             yDest + yPos - m_pImg->getHeight() / 2 );
    }
}


void CtrlSliderCursor::onUpdate( Subject<VarPercent,void*> &rVariable,
                                 void *arg  )
{
    // The position has changed
    refreshLayout();
}


void CtrlSliderCursor::CmdOverDown::execute()
{
    EvtMouse *pEvtMouse = (EvtMouse*)m_pParent->m_pEvt;

    // Compute the resize factors
    float factorX, factorY;
    m_pParent->getResizeFactors( factorX, factorY );

    // Get the position of the control
    const Position *pPos = m_pParent->getPosition();

    // Compute the offset
    int tempX, tempY;
    m_pParent->m_rCurve.getPoint( m_pParent->m_rVariable.get(), tempX, tempY );
    m_pParent->m_xOffset = pEvtMouse->getXPos() - pPos->getLeft()
                       - (int)(tempX * factorX);
    m_pParent->m_yOffset = pEvtMouse->getYPos() - pPos->getTop()
                       - (int)(tempY * factorY);

    m_pParent->captureMouse();
    m_pParent->m_pImg = m_pParent->m_pImgDown;
    m_pParent->refreshLayout();
}


void CtrlSliderCursor::CmdDownOver::execute()
{
    // Save the position
    m_pParent->m_lastPercentage = m_pParent->m_rVariable.get();

    m_pParent->releaseMouse();
    m_pParent->m_pImg = m_pParent->m_pImgUp;
    m_pParent->refreshLayout();
}


void CtrlSliderCursor::CmdUpOver::execute()
{
    m_pParent->m_pImg = m_pParent->m_pImgOver;
    m_pParent->refreshLayout();
}


void CtrlSliderCursor::CmdOverUp::execute()
{
    m_pParent->m_pImg = m_pParent->m_pImgUp;
    m_pParent->refreshLayout();
}


void CtrlSliderCursor::CmdMove::execute()
{
    EvtMouse *pEvtMouse = (EvtMouse*)m_pParent->m_pEvt;

    // Get the position of the control
    const Position *pPos = m_pParent->getPosition();

    // Compute the resize factors
    float factorX, factorY;
    m_pParent->getResizeFactors( factorX, factorY );

    // Compute the relative position of the centre of the cursor
    float relX = pEvtMouse->getXPos() - pPos->getLeft() - m_pParent->m_xOffset;
    float relY = pEvtMouse->getYPos() - pPos->getTop() - m_pParent->m_yOffset;
    // Ponderate with the resize factors
    int relXPond = (int)(relX / factorX);
    int relYPond = (int)(relY / factorY);

    // Update the position
    if( m_pParent->m_rCurve.getMinDist( relXPond, relYPond ) < RANGE )
    {
        float percentage = m_pParent->m_rCurve.getNearestPercent( relXPond,
                                                              relYPond );
        m_pParent->m_rVariable.set( percentage );
    }
    else
    {
        m_pParent->m_rVariable.set( m_pParent->m_lastPercentage );
    }
}

void CtrlSliderCursor::CmdScroll::execute()
{
    EvtScroll *pEvtScroll = (EvtScroll*)m_pParent->m_pEvt;

    int direction = pEvtScroll->getDirection();

    float percentage = m_pParent->m_rVariable.get();
    if( direction == EvtScroll::kUp )
    {
        percentage += SCROLL_STEP;
    }
    else
    {
        percentage -= SCROLL_STEP;
    }

    m_pParent->m_rVariable.set( percentage );
}


void CtrlSliderCursor::getResizeFactors( float &rFactorX,
                                         float &rFactorY ) const
{
    // Get the position of the control
    const Position *pPos = getPosition();

    rFactorX = 1.0;
    rFactorY = 1.0;

    // Compute the resize factors
    if( m_width > 0 )
    {
        rFactorX = (float)pPos->getWidth() / (float)m_width;
    }
    if( m_height > 0 )
    {
        rFactorY = (float)pPos->getHeight() / (float)m_height;
    }
}


void CtrlSliderCursor::refreshLayout()
{
    if( m_pImg )
    {
        // Compute the resize factors
        float factorX, factorY;
        getResizeFactors( factorX, factorY );

        notifyLayout( (int)(m_rCurve.getWidth() * factorX) + m_pImg->getWidth(),
                      (int)(m_rCurve.getHeight() * factorY) + m_pImg->getHeight(),
                      - m_pImg->getWidth() / 2,
                      - m_pImg->getHeight() / 2 );
    }
    else
        notifyLayout();
}


CtrlSliderBg::CtrlSliderBg( intf_thread_t *pIntf,
                            const Bezier &rCurve, VarPercent &rVariable,
                            int thickness, GenericBitmap *pBackground,
                            int nbHoriz, int nbVert, int padHoriz, int padVert,
                            VarBool *pVisible, const UString &rHelp ):
    CtrlGeneric( pIntf, rHelp, pVisible ), m_pCursor( NULL ),
    m_rVariable( rVariable ), m_thickness( thickness ), m_rCurve( rCurve ),
    m_width( rCurve.getWidth() ), m_height( rCurve.getHeight() ),
    m_pImgSeq( NULL ), m_nbHoriz( nbHoriz ), m_nbVert( nbVert ),
    m_padHoriz( padHoriz ), m_padVert( padVert ), m_bgWidth( 0 ),
    m_bgHeight( 0 ), m_position( 0 )
{
    if( pBackground )
    {
        // Build the background image sequence
        OSFactory *pOsFactory = OSFactory::instance( getIntf() );
        m_pImgSeq = pOsFactory->createOSGraphics( pBackground->getWidth(),
                                                  pBackground->getHeight() );
        m_pImgSeq->drawBitmap( *pBackground, 0, 0 );

        m_bgWidth = (pBackground->getWidth() + m_padHoriz) / nbHoriz;
        m_bgHeight = (pBackground->getHeight() + m_padVert) / nbVert;

        // Observe the position variable
        m_rVariable.addObserver( this );

        // Initial position
        m_position = (int)( m_rVariable.get() * (m_nbHoriz * m_nbVert - 1) );
    }
}


CtrlSliderBg::~CtrlSliderBg()
{
    m_rVariable.delObserver( this );
    delete m_pImgSeq;
}


bool CtrlSliderBg::mouseOver( int x, int y ) const
{
    // Compute the resize factors
    float factorX, factorY;
    getResizeFactors( factorX, factorY );

    return (m_rCurve.getMinDist( (int)(x / factorX), (int)(y / factorY),
                                 factorX, factorY ) < m_thickness );
}


void CtrlSliderBg::draw( OSGraphics &rImage, int xDest, int yDest )
{
    if( m_pImgSeq )
    {
        // Locate the right image in the background bitmap
        int x = m_bgWidth * ( m_position % m_nbHoriz );
        int y = m_bgHeight * ( m_position / m_nbHoriz );
        // Draw the background image
        rImage.drawGraphics( *m_pImgSeq, x, y, xDest, yDest,
                             m_bgWidth - m_padHoriz, m_bgHeight - m_padVert );
    }
}


void CtrlSliderBg::handleEvent( EvtGeneric &rEvent )
{
    if( rEvent.getAsString().find( "mouse:left:down" ) != string::npos )
    {
        // Compute the resize factors
        float factorX, factorY;
        getResizeFactors( factorX, factorY );

        // Get the position of the control
        const Position *pPos = getPosition();

        // Get the value corresponding to the position of the mouse
        EvtMouse &rEvtMouse = (EvtMouse&)rEvent;
        int x = rEvtMouse.getXPos();
        int y = rEvtMouse.getYPos();
        m_rVariable.set( m_rCurve.getNearestPercent(
                            (int)((x - pPos->getLeft()) / factorX),
                            (int)((y - pPos->getTop()) / factorY) ) );

        // Forward the clic to the cursor
        EvtMouse evt( getIntf(), x, y, EvtMouse::kLeft, EvtMouse::kDown );
        TopWindow *pWin = getWindow();
        if( pWin && m_pCursor )
        {
            EvtEnter evtEnter( getIntf() );
            // XXX It was not supposed to be implemented like that !!
            pWin->forwardEvent( evtEnter, *m_pCursor );
            pWin->forwardEvent( evt, *m_pCursor );
        }
    }
    else if( rEvent.getAsString().find( "scroll" ) != string::npos )
    {
        int direction = ((EvtScroll&)rEvent).getDirection();

        float percentage = m_rVariable.get();
        if( direction == EvtScroll::kUp )
        {
            percentage += SCROLL_STEP;
        }
        else
        {
            percentage -= SCROLL_STEP;
        }

        m_rVariable.set( percentage );
    }
}


void CtrlSliderBg::associateCursor( CtrlSliderCursor &rCursor )
{
    m_pCursor = &rCursor;
}


void CtrlSliderBg::onUpdate( Subject<VarPercent, void*> &rVariable, void*arg )
{
    m_position = (int)( m_rVariable.get() * (m_nbHoriz * m_nbVert - 1) );
    notifyLayout( m_bgWidth, m_bgHeight );
}


void CtrlSliderBg::getResizeFactors( float &rFactorX, float &rFactorY ) const
{
    // Get the position of the control
    const Position *pPos = getPosition();

    rFactorX = 1.0;
    rFactorY = 1.0;

    // Compute the resize factors
    if( m_width > 0 )
    {
        rFactorX = (float)pPos->getWidth() / (float)m_width;
    }
    if( m_height > 0 )
    {
        rFactorY = (float)pPos->getHeight() / (float)m_height;
    }
}

