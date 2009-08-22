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



int VoutWindow::count = 0;

VoutWindow::VoutWindow( intf_thread_t *pIntf, vout_window_t* pWnd,
                        int width, int height, GenericWindow* pParent ) :
      GenericWindow( pIntf, 0, 0, false, false, pParent ),
      m_pWnd( pWnd ), original_width( width ), original_height( height ),
      m_pParentWindow( pParent ), m_pCtrlVideo( NULL ), m_bFullscreen( false )
{
    // counter for debug
    count++;

    if( m_pWnd )
        vlc_object_hold( m_pWnd );
}


VoutWindow::~VoutWindow()
{
    if( m_pWnd )
        vlc_object_release( m_pWnd );

    count--;
    msg_Dbg( getIntf(), "VoutWindow count = %d", count );
}


void VoutWindow::setCtrlVideo( CtrlVideo* pCtrlVideo )
{
    if( pCtrlVideo )
    {
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
        setParent( VoutManager::instance( getIntf() )->getVoutMainWindow(),
                   0, 0, 0, 0 );
        m_pParentWindow =
                  VoutManager::instance( getIntf() )->getVoutMainWindow();
    }

    m_pCtrlVideo = pCtrlVideo;
}


void VoutWindow::setFullscreen( bool b_fullscreen )
{
    /*TODO: fullscreen implementation */
}


void VoutWindow::processEvent( EvtKey &rEvtKey )
{
    // Only do the action when the key is down
    if( rEvtKey.getAsString().find( "key:down") != string::npos )
    {
        vlc_value_t val;
        // Set the key
        val.i_int = rEvtKey.getKey();
        // Set the modifiers
        if( rEvtKey.getMod() & EvtInput::kModAlt )
        {
            val.i_int |= KEY_MODIFIER_ALT;
        }
        if( rEvtKey.getMod() & EvtInput::kModCtrl )
        {
            val.i_int |= KEY_MODIFIER_CTRL;
        }
        if( rEvtKey.getMod() & EvtInput::kModShift )
        {
            val.i_int |= KEY_MODIFIER_SHIFT;
        }

        var_Set( getIntf()->p_libvlc, "key-pressed", val );
    }
}

