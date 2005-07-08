/*****************************************************************************
 * ctrl_video.cpp
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN (Centrale Réseaux) and its contributors
 * $Id$
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
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

#include "ctrl_video.hpp"
#include "../src/theme.hpp"
#include "../src/vout_window.hpp"
#include "../src/os_graphics.hpp"


CtrlVideo::CtrlVideo( intf_thread_t *pIntf, const UString &rHelp,
                      VarBool *pVisible ):
    CtrlGeneric( pIntf, rHelp, pVisible ), m_pVout( NULL )
{
}


CtrlVideo::~CtrlVideo()
{
    if( m_pVout )
    {
        delete m_pVout;
    }
}


void CtrlVideo::handleEvent( EvtGeneric &rEvent )
{
}


bool CtrlVideo::mouseOver( int x, int y ) const
{
    return false;
}


void CtrlVideo::onResize()
{
    const Position *pPos = getPosition();
    if( pPos && m_pVout )
    {
        m_pVout->move( pPos->getLeft(), pPos->getTop() );
        m_pVout->resize( pPos->getWidth(), pPos->getHeight() );
    }
}


void CtrlVideo::draw( OSGraphics &rImage, int xDest, int yDest )
{
    GenericWindow *pParent = getWindow();
    const Position *pPos = getPosition();
    if( pParent && pPos )
    {
        // Draw a black rectangle under the video to avoid transparency
        rImage.fillRect( pPos->getLeft(), pPos->getTop(), pPos->getWidth(),
                         pPos->getHeight(), 0 );

        // Create a child window for the vout if it doesn't exist yet
        if (!m_pVout)
        {
            m_pVout = new VoutWindow( getIntf(), pPos->getLeft(),
                                      pPos->getTop(), false, false, *pParent );
            m_pVout->resize( pPos->getWidth(), pPos->getHeight() );
            m_pVout->show();
        }
    }
}
