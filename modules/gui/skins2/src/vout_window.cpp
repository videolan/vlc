/*****************************************************************************
 * vout_window.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
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

#include "vout_window.hpp"
#include "vout_manager.hpp"
#include "vlcproc.hpp"
#include "theme.hpp"
#include "os_factory.hpp"
#include "os_graphics.hpp"
#include "os_window.hpp"
#include "../events/evt_key.hpp"

#include <vlc_keys.h>


VoutWindow::VoutWindow( intf_thread_t *pIntf, vout_window_t* pWnd,
                        int width, int height, GenericWindow* pParent ) :
      GenericWindow( pIntf, 0, 0, false, false, pParent,
                     GenericWindow::VoutWindow ),
      m_pWnd( pWnd ), original_width( width ), original_height( height ),
      m_pCtrlVideo( NULL ), m_pParentWindow( pParent )
{
    if( m_pWnd )
    {
        vlc_object_hold( m_pWnd );

#ifdef X11_SKINS
        m_pWnd->handle.xid = getOSHandle();
        m_pWnd->display.x11 = NULL;
#else
        m_pWnd->handle.hwnd = getOSHandle();
#endif
    }
}


VoutWindow::~VoutWindow()
{
    if( m_pWnd )
    {
        vlc_object_release( m_pWnd );
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

        show();
    }
    else
    {
        hide();
        int w = VoutManager::instance( getIntf() )->getVoutMainWindow()->getWidth();
        int h = VoutManager::instance( getIntf() )->getVoutMainWindow()->getHeight();

        setParent( VoutManager::instance( getIntf() )->getVoutMainWindow(),
                   0, 0, w, h );
        m_pParentWindow =
                  VoutManager::instance( getIntf() )->getVoutMainWindow();
        show();
    }

    m_pCtrlVideo = pCtrlVideo;
}


void VoutWindow::processEvent( EvtKey &rEvtKey )
{
    // Only do the action when the key is down
    if( rEvtKey.getKeyState() == EvtKey::kDown )
        var_SetInteger( getIntf()->p_libvlc, "key-pressed",
                         rEvtKey.getModKey() );
}

