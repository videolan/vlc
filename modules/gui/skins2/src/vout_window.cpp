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

int VoutWindow::count = 0;

VoutWindow::VoutWindow( intf_thread_t *pIntf, vout_thread_t* pVout,
                        int width, int height, GenericWindow* pParent ) :
      GenericWindow( pIntf, 0, 0, false, false, pParent ),
      m_pVout( pVout ), original_width( width ), original_height( height ),
      m_pParentWindow( pParent ), m_pImage( NULL )
{
    // counter for debug
    count++;

    if( m_pVout )
        vlc_object_hold( m_pVout );

    // needed on MS-Windows to prevent vlc hanging
    show();
}


VoutWindow::~VoutWindow()
{
    delete m_pImage;
    if( m_pVout )
        vlc_object_release( m_pVout );

    count--;
    msg_Dbg( getIntf(), "VoutWindow count = %d", count );
}


void VoutWindow::resize( int width, int height )
{
    // don't try to resize with zero value
    if( !width || !height )
        return;

    // Get the OSFactory
    OSFactory *pOsFactory = OSFactory::instance( getIntf() );

    // Recreate the image
    delete m_pImage;
    m_pImage = pOsFactory->createOSGraphics( width, height );
    // Draw a black rectangle
    m_pImage->fillRect( 0, 0, width, height, 0 );

    // Resize the window
    GenericWindow::resize( width, height );
}


void VoutWindow::refresh( int left, int top, int width, int height )
{
    if( m_pImage )
    {
        if( !m_pCtrlVideo )
        {
            m_pImage->copyToWindow( *getOSWindow(), left, top,
                                    width, height, left, top );
        }
    }
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
    }
    else
    {
        setParent( VoutManager::instance( getIntf() )->getVoutMainWindow(),
                   0, 0, 0, 0 );
        m_pParentWindow =
                  VoutManager::instance( getIntf() )->getVoutMainWindow();
    }

    m_pCtrlVideo = pCtrlVideo;
}

