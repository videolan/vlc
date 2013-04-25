/*****************************************************************************
 * ctrl_slider.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
 *          Erwan Tulou      <erwan10 At videolan doT org>
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

#include "ctrl_slider.hpp"
#include "../events/evt_enter.hpp"
#include "../events/evt_mouse.hpp"
#include "../events/evt_scroll.hpp"
#include "../src/generic_bitmap.hpp"
#include "../src/scaled_bitmap.hpp"
#include "../src/top_window.hpp"
#include "../src/os_factory.hpp"
#include "../src/os_graphics.hpp"
#include "../utils/var_percent.hpp"


static inline float scroll( bool up, float pct, float step )
{
    return pct + ( up ? step : -step );
}


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
    m_xPosition( 0 ), m_yPosition( 0 ),
    m_cmdOverDown( this ), m_cmdDownOver( this ),
    m_cmdOverUp( this ), m_cmdUpOver( this ),
    m_cmdMove( this ), m_cmdScroll( this ),
    m_lastCursorRect(), m_xOffset( 0 ), m_yOffset( 0 ),
    m_pEvt( NULL ), m_rCurve( rCurve ),
    m_pImgUp( rBmpUp.getGraphics() ),
    m_pImgOver( rBmpOver.getGraphics() ),
    m_pImgDown( rBmpDown.getGraphics() ),
    m_pImg( m_pImgUp )
{
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

    // Observe the position variable
    m_rVariable.addObserver( this );
}


CtrlSliderCursor::~CtrlSliderCursor()
{
    m_rVariable.delObserver( this );
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

    return false;
}


void CtrlSliderCursor::draw( OSGraphics &rImage, int xDest, int yDest, int w, int h )
{
    if( m_pImg )
    {
        // Draw the current image
        rect inter;
        rect clip( xDest, yDest, w, h);

        if( rect::intersect( m_lastCursorRect, clip, &inter ) )
            rImage.drawGraphics( *m_pImg,
                             inter.x - m_lastCursorRect.x,
                             inter.y - m_lastCursorRect.y,
                             inter.x, inter.y, inter.width, inter.height );
    }
}


void CtrlSliderCursor::onPositionChange()
{
    m_lastCursorRect = getCurrentCursorRect();
}


void CtrlSliderCursor::onResize()
{
    onPositionChange();
}


void CtrlSliderCursor::notifyLayout( int width, int height,
                                     int xOffSet, int yOffSet )
{
    if( width > 0 && height > 0 )
    {
        CtrlGeneric::notifyLayout( width, height, xOffSet, yOffSet );
    }
    else
    {
        onPositionChange();
        const Position *pPos = getPosition();
        CtrlGeneric::notifyLayout( m_lastCursorRect.width,
                                   m_lastCursorRect.height,
                                   m_lastCursorRect.x - pPos->getLeft(),
                                   m_lastCursorRect.y - pPos->getTop() );
    }
}


void CtrlSliderCursor::onUpdate( Subject<VarPercent> &rVariable, void *arg  )
{
    (void)rVariable; (void)arg;
    // The position has changed
    refreshLayout( false );
}


void CtrlSliderCursor::CmdOverDown::execute()
{
    EvtMouse *pEvtMouse = static_cast<EvtMouse*>(m_pParent->m_pEvt);

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

    if( m_pParent->m_pImg == m_pParent->m_pImgDown )
        return;
    m_pParent->m_pImg = m_pParent->m_pImgDown;
    m_pParent->refreshLayout();
}


void CtrlSliderCursor::CmdDownOver::execute()
{
    m_pParent->releaseMouse();

    if( m_pParent->m_pImg == m_pParent->m_pImgUp )
        return;
    m_pParent->m_pImg = m_pParent->m_pImgUp;
    m_pParent->refreshLayout();
}


void CtrlSliderCursor::CmdUpOver::execute()
{
    if( m_pParent->m_pImg == m_pParent->m_pImgOver )
        return;
    m_pParent->m_pImg = m_pParent->m_pImgOver;
    m_pParent->refreshLayout();
}


void CtrlSliderCursor::CmdOverUp::execute()
{
    if( m_pParent->m_pImg == m_pParent->m_pImgUp )
        return;
    m_pParent->m_pImg = m_pParent->m_pImgUp;
    m_pParent->refreshLayout();
}


void CtrlSliderCursor::CmdMove::execute()
{
    EvtMouse *pEvtMouse = static_cast<EvtMouse*>(m_pParent->m_pEvt);

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

    float percentage =
        m_pParent->m_rCurve.getNearestPercent( relXPond, relYPond );
    m_pParent->m_rVariable.set( percentage );
}

void CtrlSliderCursor::CmdScroll::execute()
{
    int dir = static_cast<EvtScroll*>(m_pParent->m_pEvt)->getDirection();
    m_pParent->m_rVariable.set( scroll( EvtScroll::kUp == dir,
                                        m_pParent->m_rVariable.get(),
                                        m_pParent->m_rVariable.getStep()) );
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
        rFactorX = (float)pPos->getWidth() / (float)m_width;
    if( m_height > 0 )
        rFactorY = (float)pPos->getHeight() / (float)m_height;
}


void CtrlSliderCursor::refreshLayout( bool force )
{
    rect currRect = getCurrentCursorRect();
    if( !force && currRect == m_lastCursorRect )
        return;

    rect join;
    if( rect::join( m_lastCursorRect, currRect, &join ) )
    {
        m_lastCursorRect = currRect;

        const Position *pPos = getPosition();
        notifyLayout( join.width, join.height,
                      join.x - pPos->getLeft(),
                      join.y - pPos->getTop() );
    }
}


rect CtrlSliderCursor::getCurrentCursorRect()
{
    const Position *pPos = getPosition();

    // Compute the position of the cursor
    int xPos, yPos;
    m_rCurve.getPoint( m_rVariable.get(), xPos, yPos );

    // Compute the resize factors
    float factorX, factorY;
    getResizeFactors( factorX, factorY );
    xPos = (int)(xPos * factorX);
    yPos = (int)(yPos * factorY);

    int x = pPos->getLeft() + xPos - m_pImg->getWidth() / 2;
    int y = pPos->getTop() + yPos - m_pImg->getHeight() / 2;

    return rect( x, y, m_pImg->getWidth(), m_pImg->getHeight() );
}


CtrlSliderBg::CtrlSliderBg( intf_thread_t *pIntf,
                            const Bezier &rCurve, VarPercent &rVariable,
                            int thickness, GenericBitmap *pBackground,
                            int nbHoriz, int nbVert, int padHoriz, int padVert,
                            VarBool *pVisible, const UString &rHelp ):
    CtrlGeneric( pIntf, rHelp, pVisible ), m_pCursor( NULL ),
    m_rVariable( rVariable ), m_thickness( thickness ), m_rCurve( rCurve ),
    m_width( rCurve.getWidth() ), m_height( rCurve.getHeight() ),
    m_pImgSeq( pBackground ), m_pScaledBmp( NULL ),
    m_nbHoriz( nbHoriz ), m_nbVert( nbVert ),
    m_padHoriz( padHoriz ), m_padVert( padVert ),
    m_bgWidth( 0 ), m_bgHeight( 0 ), m_position( 0 )
{
    if( m_pImgSeq )
    {
        // Observe the position variable
        m_rVariable.addObserver( this );

        // Initial position
        m_position = (int)( m_rVariable.get() * (m_nbHoriz * m_nbVert - 1) );
    }
}


CtrlSliderBg::~CtrlSliderBg()
{
    if( m_pImgSeq )
        m_rVariable.delObserver( this );

    delete m_pScaledBmp;
}


bool CtrlSliderBg::mouseOver( int x, int y ) const
{
    // Compute the resize factors
    float factorX, factorY;
    getResizeFactors( factorX, factorY );

    if( m_pScaledBmp )
    {
        // background size that is displayed
        int width = m_bgWidth - (int)(m_padHoriz * factorX);
        int height = m_bgHeight - (int)(m_padVert * factorY);

        return x >= 0 && x < width &&
               y >= 0 && y < height;
    }
    else
    {
        return m_rCurve.getMinDist( (int)(x / factorX), (int)(y / factorY),
                                    factorX, factorY ) < m_thickness;
    }
}


void CtrlSliderBg::draw( OSGraphics &rImage, int xDest, int yDest, int w, int h )
{
    if( !m_pScaledBmp || m_bgWidth <= 0 || m_bgHeight <= 0 )
        return;

    // Compute the resize factors
    float factorX, factorY;
    getResizeFactors( factorX, factorY );

    // Locate the right image in the background bitmap
    int x = m_bgWidth * ( m_position % m_nbHoriz );
    int y = m_bgHeight * ( m_position / m_nbHoriz );

    // Draw the background image
    const Position *pPos = getPosition();
    rect region( pPos->getLeft(), pPos->getTop(),
                 m_bgWidth - (int)(m_padHoriz * factorX),
                 m_bgHeight - (int)(m_padVert * factorY) );
    rect clip( xDest, yDest, w, h );
    rect inter;
    if( rect::intersect( region, clip, &inter ) )
        rImage.drawBitmap( *m_pScaledBmp,
                           x + inter.x - region.x,
                           y + inter.y - region.y,
                           inter.x, inter.y,
                           inter.width, inter.height );
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
        int dir = static_cast<EvtScroll*>(&rEvent)->getDirection();
        m_rVariable.set( scroll( EvtScroll::kUp == dir,
                                 m_rVariable.get(), m_rVariable.getStep() ) );
    }
}


void CtrlSliderBg::onPositionChange()
{
    if( m_pImgSeq )
    {
        setCurrentImage();
    }
}


void CtrlSliderBg::onResize()
{
    if( m_pImgSeq )
    {
        setCurrentImage();
    }
}


void CtrlSliderBg::associateCursor( CtrlSliderCursor &rCursor )
{
    m_pCursor = &rCursor;
}


void CtrlSliderBg::notifyLayout( int width, int height,
                                 int xOffSet, int yOffSet )
{
    if( width > 0 && height > 0 )
    {
        CtrlGeneric::notifyLayout( width, height, xOffSet, yOffSet );
    }
    else
    {
        // Compute the resize factors
        float factorX, factorY;
        getResizeFactors( factorX, factorY );
        // real background size
        int width = m_bgWidth - (int)(m_padHoriz * factorX);
        int height = m_bgHeight - (int)(m_padVert * factorY);
        CtrlGeneric::notifyLayout( width, height );
    }
}


void CtrlSliderBg::onUpdate( Subject<VarPercent> &rVariable, void*arg )
{
    (void)rVariable; (void)arg;
    int position = (int)( m_rVariable.get() * (m_nbHoriz * m_nbVert - 1) );
    if( position == m_position )
        return;

    m_position = position;

    // redraw the entire control
    notifyLayout();
}


void CtrlSliderBg::getResizeFactors( float &rFactorX, float &rFactorY ) const
{
    // Get the position of the control
    const Position *pPos = getPosition();

    rFactorX = 1.0;
    rFactorY = 1.0;

    // Compute the resize factors
    if( m_width > 0 )
        rFactorX = (float)pPos->getWidth() / (float)m_width;
    if( m_height > 0 )
        rFactorY = (float)pPos->getHeight() / (float)m_height;
}


void CtrlSliderBg::setCurrentImage()
{
    // Compute the resize factors
    float factorX, factorY;
    getResizeFactors( factorX, factorY );

    // Build the background image sequence
    // Note: we suppose that the last padding is not included in the
    // given image
    // TODO: we should probably change this assumption, as it would make
    // the code a bit simpler and it would be more natural for the skins
    // designers
    // Size of one elementary background image (padding included)
    m_bgWidth =
        (int)((m_pImgSeq->getWidth() + m_padHoriz) * factorX / m_nbHoriz);
    m_bgHeight =
        (int)((m_pImgSeq->getHeight() + m_padVert) * factorY / m_nbVert);

    // Rescale the image accordingly
    int width = m_bgWidth * m_nbHoriz - (int)(m_padHoriz * factorX);
    int height = m_bgHeight * m_nbVert - (int)(m_padVert * factorY);
    if( !m_pScaledBmp ||
        m_pScaledBmp->getWidth() != width ||
        m_pScaledBmp->getHeight() != height )
    {
        // scaled bitmap
        delete m_pScaledBmp;
        m_pScaledBmp = new ScaledBitmap( getIntf(), *m_pImgSeq, width, height );
    }
}
