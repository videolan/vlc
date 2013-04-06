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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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
    m_position( 0 ),
    m_width( rBmpSeq.getWidth() ), m_height( rBmpSeq.getHeight() / numImg ),
    m_pImgSeq( rBmpSeq.getGraphics() ),
    m_cmdUpDown( this ), m_cmdDownUp( this ), m_cmdMove( this )
{
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


void CtrlRadialSlider::draw( OSGraphics &rImage, int xDest, int yDest, int w, int h )
{
    const Position *pPos = getPosition();
    rect region( pPos->getLeft(), pPos->getTop(), m_width, m_height );
    rect clip( xDest, yDest, w ,h );
    rect inter;
    if( rect::intersect( region, clip, &inter ) )
        rImage.drawGraphics( *m_pImgSeq,
                              inter.x - region.x,
                              inter.y - region.y + m_position * m_height,
                              inter.x, inter.y,
                              inter.width, inter.height );
}


void CtrlRadialSlider::onUpdate( Subject<VarPercent> &rVariable, void *arg )
{
    (void)arg;
    if( &rVariable == &m_rVariable )
    {
        int position = (int)( m_rVariable.get() * ( m_numImg - 1 ) );
        if( position == m_position )
            return;

        m_position = position;
        notifyLayout( m_width, m_height );
    }
}


void CtrlRadialSlider::CmdUpDown::execute()
{
    EvtMouse *pEvtMouse = (EvtMouse*)m_pParent->m_pEvt;

    // Change the position of the cursor, in non-blocking mode
    m_pParent->setCursor( pEvtMouse->getXPos(), pEvtMouse->getYPos(), false );

    m_pParent->captureMouse();
}


void CtrlRadialSlider::CmdDownUp::execute()
{
    m_pParent->releaseMouse();
}


void CtrlRadialSlider::CmdMove::execute()
{
    EvtMouse *pEvtMouse = static_cast<EvtMouse*>(m_pParent->m_pEvt);

    // Change the position of the cursor, in blocking mode
    m_pParent->setCursor( pEvtMouse->getXPos(), pEvtMouse->getYPos(), true );
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
    int y = posY - pPos->getTop() - m_height / 2;

    // Compute the polar coordinates. angle is -(-j,OM)
    float r = sqrt((float)(x*x + y*y));
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

