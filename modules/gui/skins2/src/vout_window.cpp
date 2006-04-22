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
#include "vlcproc.hpp"
#include "os_factory.hpp"
#include "os_graphics.hpp"
#include "os_window.hpp"


VoutWindow::VoutWindow( intf_thread_t *pIntf, int left, int top,
                        bool dragDrop, bool playOnDrop,
                        GenericWindow &rParent ):
    GenericWindow( pIntf, left, top, dragDrop, playOnDrop,
                   &rParent ), m_pImage( NULL )
{
}


VoutWindow::~VoutWindow()
{
    if( m_pImage )
    {
        delete m_pImage;
    }

    // Get the VlcProc
    VlcProc *pVlcProc = getIntf()->p_sys->p_vlcProc;

    // Reparent the video output
    if( pVlcProc && pVlcProc->isVoutUsed() )
    {
        pVlcProc->dropVout();
    }
}


void VoutWindow::resize( int width, int height )
{
    // Get the OSFactory
    OSFactory *pOsFactory = OSFactory::instance( getIntf() );

    // Recreate the image
    if( m_pImage )
    {
        delete m_pImage;
    }
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
        // Get the VlcProc
        VlcProc *pVlcProc = getIntf()->p_sys->p_vlcProc;

        // Refresh only when there is no video!
        if( pVlcProc && !pVlcProc->isVoutUsed() )
        {
            m_pImage->copyToWindow( *getOSWindow(), left, top,
                                    width, height, left, top );
        }
    }
}

