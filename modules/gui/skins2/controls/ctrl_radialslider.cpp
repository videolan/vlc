/*****************************************************************************
 * ctrl_radialslider.cpp
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

#include <math.h>
#include "ctrl_radialslider.hpp"
#include "../events/evt_mouse.hpp"
#include "../src/generic_bitmap.hpp"
#include "../src/generic_window.hpp"
#include "../src/os_factory.hpp"
#include "../src/os_graphics.hpp"
#include "../utils/position.hpp"
#include "../utils/var_percent.hpp"


CtrlRadialSlider::CtrlRadialSlider( intf_thread_t *pIntf,
                                    const GenericBitmap &rBmpSeq, int numImg,
                                    VarPercent &rVariable, float minAngle,
                                    float maxAngle, const UString &rHelp,
                                    VarBool *pVisible ):
    CtrlGeneric( pIntf, rHelp, pVisible ), m_fsm( pIntf ), m_numImg( numImg ),
    m_rVariable( rVariable ), m_minAngle( minAngle ), m_maxAngle( maxAngle ),
    m_cmdUpDown( this, &transUpDown ), m_cmdDownUp( this, &transDownUp ),
    m_cmdMove( this, &transMove ), m_position( 0 ), m_lastPos( 0 )
{
    // Build the images of the sequence
    OSFactory *pOsFactory = OSFactory::instance( getIntf() );
    m_pImgSeq = pOsFactory->createOSGraphics( rBmpSeq.getWidth(),
                                              rBmpSeq.getHeight() );
    m_pImgSeq->drawBitmap( rBmpSeq, 0, 0 );

    m_width = rBmpSeq.getWidth();
    m_height = rBmpSeq.getHeight() / numImg;

    // States
    m_fsm.addState( "up" );
    m_fsm.addState( "down" );

    // Transitions
    m_fsm.addTransition( "up", "mouse:left:down", "down", &m_cmdUpDown );
    m_fsm.addTransition( "down", "mouse:left:up", "up", &m_cmdDownUp );
    m_fsm.addTransition( "down", "motion", "down", &m_cmdMove );

    // Initial state
    m_fsm.setState( "up" );

    // Observe the variable
    m_rVariable.addObserver( this );
}


CtrlRadialSlider::~CtrlRadialSlider()
{
    m_rVariable.delObserver( this );
    SKINS_DELETE( m_pImgSeq );
}


void CtrlRadialSlider::handleEvent( EvtGeneric &rEvent )
{
    // Save the event to use it in callbacks
    m_pEvt = &rEvent;

    m_fsm.handleTransition( rEvent.getAsString() );
}


bool CtrlRadialSlider::mouseOver( int x, int y ) const
{
    return m_pImgSeq->hit( x, y + m_position * m_height );
}


void CtrlRadialSlider::draw( OSGraphics &rImage, int xDest, int yDest )
{
    rImage.drawGraphics( *m_pImgSeq, 0, m_position * m_height, xDest, yDest,
                         m_width, m_height );
}


void CtrlRadialSlider::onUpdate( Subject<VarPercent> &rVariable )
{
    m_position = (int)( m_rVariable.get() * m_numImg );
    notifyLayout( m_width, m_height );
}


void CtrlRadialSlider::transUpDown( SkinObject *pCtrl )
{
    CtrlRadialSlider *pThis = (CtrlRadialSlider*)pCtrl;

    EvtMouse *pEvtMouse = (EvtMouse*)pThis->m_pEvt;

    // Change the position of the cursor, in non-blocking mode
    pThis->setCursor( pEvtMouse->getXPos(), pEvtMouse->getYPos(), false );

    pThis->captureMouse();
}


void CtrlRadialSlider::transDownUp( SkinObject *pCtrl )
{
    CtrlRadialSlider *pThis = (CtrlRadialSlider*)pCtrl;

    pThis->releaseMouse();
}


void CtrlRadialSlider::transMove( SkinObject *pCtrl )
{
    CtrlRadialSlider *pThis = (CtrlRadialSlider*)pCtrl;

    EvtMouse *pEvtMouse = (EvtMouse*)pThis->m_pEvt;

    // Change the position of the cursor, in blocking mode
    pThis->setCursor( pEvtMouse->getXPos(), pEvtMouse->getYPos(), true );
}


void CtrlRadialSlider::setCursor( int posX, int posY, bool blocking )
{
    // Get the position of the control
    const Position *pPos = getPosition();
    if( !pPos )
    {
        return;
    }

    // Compute the position relative to the center
    int x = posX - pPos->getLeft() - m_width / 2;
    int y = posY - pPos->getTop() - m_width / 2;

    // Compute the polar coordinates. angle is -(-j,OM)
    float r = sqrt(x*x + y*y);
    if( r == 0 )
    {
        return;
    }
    float angle = acos(y/r);
    if( x > 0 )
    {
        angle = 2*M_PI - angle;
    }

    if( angle >= m_minAngle && angle <= m_maxAngle )
    {
        float newVal = (angle - m_minAngle) / (m_maxAngle - m_minAngle);
        // Avoid too fast moves of the cursor if blocking mode
        if( !blocking || fabs( m_rVariable.get() - newVal ) < 0.5 )
        {
            m_rVariable.set( newVal );
        }
    }
}

