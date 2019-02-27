/*****************************************************************************
 * vout_window.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
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

#include "vout_window.hpp"
#include "vout_manager.hpp"
#include "vlcproc.hpp"
#include "theme.hpp"
#include "os_factory.hpp"
#include "os_graphics.hpp"
#include "os_timer.hpp"
#include "os_window.hpp"
#include "../events/evt_key.hpp"
#include "../events/evt_motion.hpp"
#include "../events/evt_mouse.hpp"

#include <vlc_actions.h>


VoutWindow::VoutWindow( intf_thread_t *pIntf, vout_window_t* pWnd,
                        int width, int height, GenericWindow* pParent ) :
      GenericWindow( pIntf, 0, 0, false, false, pParent,
                     GenericWindow::VoutWindow ),
      m_pWnd( pWnd ), original_width( width ), original_height( height ),
      m_pCtrlVideo( NULL ), m_pParentWindow( pParent ),
      mouse_hide_timeout( var_InheritInteger( pWnd, "mouse-hide-timeout" ) ),
      m_cmdHideMouse( this )
{
    OSFactory *pOsFactory = OSFactory::instance( pIntf );

    if( m_pWnd )
    {
        updateWindowConfiguration( m_pWnd );

        m_pTimer = pOsFactory->createOSTimer( m_cmdHideMouse );
    }
}


VoutWindow::~VoutWindow()
{
    if( m_pWnd )
    {
        delete m_pTimer;
    }
}


void VoutWindow::setCtrlVideo( CtrlVideo* pCtrlVideo )
{
    if( pCtrlVideo )
    {
        hide();
        const Position *pPos = pCtrlVideo->getPosition();
        int x = pPos->getLeft();
        int y = pPos->getTop();
        int w = pPos->getWidth();
        int h = pPos->getHeight();

        setParent( pCtrlVideo->getWindow(), x, y, w, h );
        m_pParentWindow = pCtrlVideo->getWindow();

        resize( w, h );
        show();
    }
    else
    {
        hide();
        setParent( VoutManager::instance( getIntf() )->getVoutMainWindow() );
        m_pParentWindow =
                  VoutManager::instance( getIntf() )->getVoutMainWindow();
        show();
    }

    m_pCtrlVideo = pCtrlVideo;
}


void VoutWindow::resize( int width, int height )
{
    GenericWindow::resize( width, height );

    if( m_pWnd )
        vout_window_ReportSize( m_pWnd, width, height );
}


void VoutWindow::processEvent( EvtKey &rEvtKey )
{
    // Only do the action when the key is down
    if( rEvtKey.getKeyState() == EvtKey::kDown )
        getIntf()->p_sys->p_dialogs->sendKey( rEvtKey.getModKey() );
}


void VoutWindow::processEvent( EvtScroll &rEvtScroll )
{
    int i = (rEvtScroll.getDirection() == EvtScroll::kUp ?
            KEY_MOUSEWHEELUP : KEY_MOUSEWHEELDOWN) | rEvtScroll.getMod();

    getIntf()->p_sys->p_dialogs->sendKey( i );
}


void VoutWindow::processEvent( EvtMotion &rEvtMotion )
{
    int x = rEvtMotion.getXPos() - m_pParentWindow->getLeft() - getLeft();
    int y = rEvtMotion.getYPos() - m_pParentWindow->getTop() - getTop();

    vout_window_ReportMouseMoved( m_pWnd, x, y );
    showMouse();
}


void VoutWindow::processEvent( EvtMouse &rEvtMouse )
{
    int button = -1;
    if( rEvtMouse.getButton() == EvtMouse::kLeft )
        button = 0;
    else if( rEvtMouse.getButton() == EvtMouse::kMiddle )
        button = 1;
    else if( rEvtMouse.getButton() == EvtMouse::kRight )
        button = 2;

    if( rEvtMouse.getAction() == EvtMouse::kDown )
        vout_window_ReportMousePressed( m_pWnd, button );
    else if( rEvtMouse.getAction() == EvtMouse::kUp )
        vout_window_ReportMouseReleased( m_pWnd, button );
    else if( rEvtMouse.getAction() == EvtMouse::kDblClick )
        vout_window_ReportMouseDoubleClick( m_pWnd, button );
    showMouse();
}


void VoutWindow::showMouse()
{
    m_pTimer->start( mouse_hide_timeout, true );
    hideMouse( false );
}


void VoutWindow::hideMouse( bool hide )
{
    VoutManager::instance( getIntf() )->hideMouseWnd( m_pWnd, hide );
}


void VoutWindow::CmdHideMouse::execute()
{
    m_pParent->hideMouse( true );
}
