/*****************************************************************************
 * fsc_window.cpp
 *****************************************************************************
 * Copyright (C) 2010 the VideoLAN team
 * $Id$
 *
 * Author: Erwan Tulou      <erwan10 at videolan Dot Org>
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
 * * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "fsc_window.hpp"
#include "vout_manager.hpp"
#include "os_factory.hpp"
#include "os_timer.hpp"
#include "vlcproc.hpp"

/**
 * Fading out is computed in the following way:
 *    - a timer is fired with a given period (m_delay)
 *    - a total of FSC_COUNT transitions are processed before
 *      hiding the controller
 *    - transparency is changed in the following way :
 *         - unaltered for the first FSC_COUNT2 transitions
 *         - then linear decrease from m_opacity to 0 till
 *           FSC_COUNT is reached.
 **/
#define FSC_DELAY    50      // period for fading out (in msec)
#define FSC_COUNT    60      // total # of transitions for fading out
#define FSC_COUNT2   20      // # of transitions with opacity unaltered

FscWindow::FscWindow( intf_thread_t *pIntf, int left, int top,
                      WindowManager &rWindowManager,
                      bool dragDrop, bool playOnDrop, bool visible ) :
    TopWindow( pIntf, left, top, rWindowManager, dragDrop,
               playOnDrop, false, GenericWindow::FscWindow ),
    m_pTimer( NULL ), m_count( 0 ),
    m_cmdFscHide( this )
{
    (void)visible;
    m_pTimer = OSFactory::instance( getIntf() )->createOSTimer( m_cmdFscHide );

    VarBool &rFullscreen = VlcProc::instance( getIntf() )->getFullscreenVar();
    rFullscreen.addObserver( this );

    // opacity overridden by user
    m_opacity = 255 * var_InheritFloat( getIntf(), "qt-fs-opacity" );

    // fullscreen-controller timeout overridden by user
    m_delay = var_InheritInteger( getIntf(), "mouse-hide-timeout" ) / FSC_COUNT;
    if( m_delay <= 0 )
        m_delay = FSC_DELAY;

    /// activation overridden by user
    m_enabled = var_InheritBool( getIntf(), "qt-fs-controller" );

    // register Fsc
    VoutManager::instance( getIntf())->registerFSC( this );
}


FscWindow::~FscWindow()
{
    // unregister Fsc
    VoutManager::instance( getIntf())->registerFSC( NULL );

    VarBool &rFullscreen = VlcProc::instance( getIntf() )->getFullscreenVar();
    rFullscreen.delObserver( this );

    delete m_pTimer;
}


void FscWindow::onMouseMoved( )
{
    VarBool &rFullscreen = VlcProc::instance( getIntf() )->getFullscreenVar();
    if( rFullscreen.get()  )
    {
        show();
        if( m_count < FSC_COUNT - FSC_COUNT2 )
        {
            m_pTimer->stop();
            m_count = FSC_COUNT;
            setOpacity( m_opacity );
            m_pTimer->start( m_delay, false );
        }
    }
}


void FscWindow::onTimerExpired()
{
    if( m_count )
    {
        if( m_count < FSC_COUNT - FSC_COUNT2 )
            setOpacity( m_opacity * m_count / (FSC_COUNT - FSC_COUNT2 ) );
        m_count--;
    }

    if( !m_count )
    {
        hide();
    }
}


void FscWindow::processEvent( EvtMotion &rEvtMotion )
{
    if( m_count )
    {
        m_pTimer->stop();
        m_count = 0;
        setOpacity( m_opacity );
    }

    TopWindow::processEvent( rEvtMotion  );
}


void FscWindow::processEvent( EvtLeave &rEvtLeave )
{
    if( m_count )
    {
        m_pTimer->stop();
        m_count = 0;
    }

    m_count = FSC_COUNT;
    setOpacity( m_opacity );
    m_pTimer->start( m_delay, false );

    TopWindow::processEvent( rEvtLeave  );
}


void FscWindow::onUpdate( Subject<VarBool> &rVariable, void *arg  )
{
    VarBool &rFullscreen = VlcProc::instance( getIntf() )->getFullscreenVar();
    if( &rVariable == &rFullscreen )
    {
        if( !rFullscreen.get() )
        {
            hide();
        }
    }
    TopWindow::onUpdate( rVariable, arg );
}


void FscWindow::innerShow()
{
    if( !m_enabled )
        return;

    TopWindow::innerShow();

    m_count = FSC_COUNT;
    setOpacity( m_opacity );
    m_pTimer->start( m_delay, false );
}


void FscWindow::innerHide()
{
    m_pTimer->stop();
    m_count = 0;

    TopWindow::innerHide();
}


void FscWindow::moveTo( int x, int y, int width, int height )
{
    // relocate the fs controller
    // (centered horizontally and lowered vertically with a 3% margin)
    int x_fsc = x + ( width - getWidth() ) / 2;
    int y_fsc = y + height - getHeight() - height * 3 / 100;
    move( x_fsc, y_fsc );
}


void FscWindow::CmdFscHide::execute()
{
    m_pParent->onTimerExpired();
}
