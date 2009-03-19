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
#include "../src/vout_manager.hpp"
#include "../src/window_manager.hpp"
#include "../commands/async_queue.hpp"
#include "../commands/cmd_resize.hpp"


CtrlVideo::CtrlVideo( intf_thread_t *pIntf, GenericLayout &rLayout,
                      bool autoResize, const UString &rHelp,
                      VarBool *pVisible ):
    CtrlGeneric( pIntf, rHelp, pVisible ), m_rLayout( rLayout ),
    m_xShift( 0 ), m_yShift( 0 ), m_bAutoResize( autoResize ),
    m_pVoutWindow( NULL ), m_bIsUseable( false )
{
    // Observe the vout size variable if the control is auto-resizable
    if( m_bAutoResize )
    {
        VarBox &rVoutSize = VlcProc::instance( pIntf )->getVoutSizeVar();
        rVoutSize.addObserver( this );
    }

    // observe visibility variable
    if( m_pVisible )
        m_pVisible->addObserver( this );
}


CtrlVideo::~CtrlVideo()
{
    VarBox &rVoutSize = VlcProc::instance( getIntf() )->getVoutSizeVar();
    rVoutSize.delObserver( this );

    //m_pLayout->getActiveVar().delObserver( this );

    if( m_pVisible )
        m_pVisible->delObserver( this );
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
    if( pPos && m_pVoutWindow )
    {
        m_pVoutWindow->move( pPos->getLeft(), pPos->getTop() );
        m_pVoutWindow->resize( pPos->getWidth(), pPos->getHeight() );
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


void CtrlVideo::setLayout( GenericLayout *pLayout,
                           const Position &rPosition )
{
    CtrlGeneric::setLayout( pLayout, rPosition );
    m_pLayout->getActiveVar().addObserver( this );

    m_bIsUseable = isVisible() && m_pLayout->getActiveVar().get();

    // register Video Control
    VoutManager::instance( getIntf() )->registerCtrlVideo( this );

    msg_Dbg( getIntf(),"New VideoControl detected(%p), useability=%s",
                           this, m_bIsUseable ? "true" : "false" );
}


void CtrlVideo::resizeControl( int width, int height )
{
    int newWidth = width + m_xShift;
    int newHeight = height + m_yShift;

    // Create a resize command
    // FIXME: this way of getting the window manager kind of sucks
    WindowManager &rWindowManager =
        getIntf()->p_sys->p_theme->getWindowManager();
    rWindowManager.startResize( m_rLayout, WindowManager::kResizeSE );
    CmdGeneric *pCmd = new CmdResize( getIntf(), rWindowManager,
                                      m_rLayout, newWidth, newHeight );
    // Push the command in the asynchronous command queue
    AsyncQueue *pQueue = AsyncQueue::instance( getIntf() );
    pQueue->push( CmdGenericPtr( pCmd ), false );

    // FIXME: this should be a command too
    rWindowManager.stopResize();

    pCmd = new CmdResizeInnerVout( getIntf(), this );
    pQueue->push( CmdGenericPtr( pCmd ), false );

    TopWindow* pWin = getWindow();
    rWindowManager.show( *pWin );
}


void CtrlVideo::onUpdate( Subject<VarBox> &rVoutSize, void *arg )
{
    int newWidth = ((VarBox&)rVoutSize).getWidth() + m_xShift;
    int newHeight = ((VarBox&)rVoutSize).getHeight() + m_yShift;

    resizeControl( newWidth, newHeight );
}


void CtrlVideo::onUpdate( Subject<VarBool> &rVariable, void *arg  )
{
    // Visibility changed
    if( &rVariable == m_pVisible )
    {
        msg_Dbg( getIntf(), "VideoCtrl : Visibility changed (visible=%d)",
                                  isVisible() );
    }

    // Active Layout changed
    if( &rVariable == &m_pLayout->getActiveVar() )
    {
        msg_Dbg( getIntf(), "VideoCtrl : Active Layout changed (isActive=%d)",
                      m_pLayout->getActiveVar().get() );
    }

    m_bIsUseable = isVisible() && m_pLayout->getActiveVar().get();

    if( m_bIsUseable && !isUsed() )
    {
        VoutManager::instance( getIntf() )->requestVout( this );
    }
    else if( !m_bIsUseable && isUsed() )
    {
        VoutManager::instance( getIntf() )->discardVout( this );
    }
}

void CtrlVideo::attachVoutWindow( VoutWindow* pVoutWindow )
{
    int width = pVoutWindow->getOriginalWidth();
    int height = pVoutWindow->getOriginalHeight();

    WindowManager &rWindowManager =
        getIntf()->p_sys->p_theme->getWindowManager();
    TopWindow* pWin = getWindow();
    rWindowManager.show( *pWin );

    if( m_bAutoResize && width && height )
    {
        int newWidth = width + m_xShift;
        int newHeight = height + m_yShift;

        rWindowManager.startResize( m_rLayout, WindowManager::kResizeSE );
        rWindowManager.resize( m_rLayout, newWidth, newHeight );
        rWindowManager.stopResize();
    }

    pVoutWindow->setCtrlVideo( this );

    m_pVoutWindow = pVoutWindow;
}


void CtrlVideo::detachVoutWindow( )
{
    m_pVoutWindow->setCtrlVideo( NULL );
    m_pVoutWindow = NULL;
}


void CtrlVideo::resizeInnerVout( )
{
    WindowManager &rWindowManager =
         getIntf()->p_sys->p_theme->getWindowManager();
    TopWindow* pWin = getWindow();

    const Position *pPos = getPosition();

    m_pVoutWindow->resize( pPos->getWidth(), pPos->getHeight() );
    m_pVoutWindow->move( pPos->getLeft(), pPos->getTop() );

    rWindowManager.show( *pWin );
    m_pVoutWindow->show();
}

