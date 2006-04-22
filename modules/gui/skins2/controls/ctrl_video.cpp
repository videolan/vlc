/*****************************************************************************
 * ctrl_video.cpp
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "ctrl_video.hpp"
#include "../src/theme.hpp"
#include "../src/vout_window.hpp"
#include "../src/os_graphics.hpp"
#include "../src/vlcproc.hpp"
#include "../commands/async_queue.hpp"
#include "../commands/cmd_resize.hpp"


CtrlVideo::CtrlVideo( intf_thread_t *pIntf, GenericLayout &rLayout,
                      bool autoResize, const UString &rHelp,
                      VarBool *pVisible ):
    CtrlGeneric( pIntf, rHelp, pVisible ), m_pVout( NULL ),
    m_rLayout( rLayout ), m_xShift( 0 ), m_yShift( 0 )
{
    // Observe the vout size variable if the control is auto-resizable
    if( autoResize )
    {
        VarBox &rVoutSize = VlcProc::instance( pIntf )->getVoutSizeVar();
        rVoutSize.addObserver( this );
    }
}


CtrlVideo::~CtrlVideo()
{
    VarBox &rVoutSize = VlcProc::instance( getIntf() )->getVoutSizeVar();
    rVoutSize.delObserver( this );

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


void CtrlVideo::onPositionChange()
{
    // Compute the difference between layout size and video size
    m_xShift = m_rLayout.getWidth() - getPosition()->getWidth();
    m_yShift = m_rLayout.getHeight() - getPosition()->getHeight();
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
    }
}


void CtrlVideo::onUpdate( Subject<VarBox, void *> &rVoutSize, void *arg )
{
    int newWidth = ((VarBox&)rVoutSize).getWidth() + m_xShift;
    int newHeight = ((VarBox&)rVoutSize).getHeight() + m_yShift;

    // Create a resize command
    CmdGeneric *pCmd = new CmdResize( getIntf(), m_rLayout, newWidth,
                                      newHeight );
    // Push the command in the asynchronous command queue
    AsyncQueue *pQueue = AsyncQueue::instance( getIntf() );
    pQueue->push( CmdGenericPtr( pCmd ) );
}


void CtrlVideo::setVisible( bool visible )
{
    if( visible )
    {
        GenericWindow *pParent = getWindow();
        const Position *pPos = getPosition();
        // Create a child window for the vout if it doesn't exist yet
        if( !m_pVout && pParent && pPos )
        {
            m_pVout = new VoutWindow( getIntf(), pPos->getLeft(),
                                      pPos->getTop(), false, false, *pParent );
            m_pVout->resize( pPos->getWidth(), pPos->getHeight() );
            m_pVout->show();
        }
    }
    else
    {
        delete m_pVout;
        m_pVout = NULL;
    }
}

