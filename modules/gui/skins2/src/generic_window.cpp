/*****************************************************************************
 * generic_window.cpp
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

#include "generic_window.hpp"
#include "os_window.hpp"
#include "os_factory.hpp"
#include "../events/evt_refresh.hpp"


GenericWindow::GenericWindow( intf_thread_t *pIntf, int left, int top,
                              bool dragDrop, bool playOnDrop,
                              GenericWindow *pParent ):
    SkinObject( pIntf ), m_left( left ), m_top( top ), m_width( 0 ),
    m_height( 0 ), m_varVisible( pIntf )
{
    // Get the OSFactory
    OSFactory *pOsFactory = OSFactory::instance( getIntf() );

    // Get the parent OSWindow, if any
    OSWindow *pOSParent = NULL;
    if( pParent )
    {
        pOSParent = pParent->m_pOsWindow;
    }

    // Create an OSWindow to handle OS specific processing
    m_pOsWindow = pOsFactory->createOSWindow( *this, dragDrop, playOnDrop,
                                              pOSParent );

    // Observe the visibility variable
    m_varVisible.addObserver( this );
}


GenericWindow::~GenericWindow()
{
    m_varVisible.delObserver( this );

    if( m_pOsWindow )
    {
        delete m_pOsWindow;
    }
}


void GenericWindow::processEvent( EvtRefresh &rEvtRefresh )
{
    // Refresh the given area
    refresh( rEvtRefresh.getXStart(), rEvtRefresh.getYStart(),
             rEvtRefresh.getWidth(), rEvtRefresh.getHeight() );
}


void GenericWindow::show() const
{
    m_varVisible.set( true );
}


void GenericWindow::hide() const
{
    m_varVisible.set( false );
}


void GenericWindow::move( int left, int top )
{
    // Update the window coordinates
    m_left = left;
    m_top = top;

    m_pOsWindow->moveResize( left, top, m_width, m_height );
}


void GenericWindow::resize( int width, int height )
{
    // Update the window size
    m_width = width;
    m_height = height;

    m_pOsWindow->moveResize( m_left, m_top, width, height );
}


void GenericWindow::raise() const
{
    m_pOsWindow->raise();
}


void GenericWindow::setOpacity( uint8_t value )
{
    m_pOsWindow->setOpacity( value );
}


void GenericWindow::toggleOnTop( bool onTop ) const
{
    m_pOsWindow->toggleOnTop( onTop );
}


void GenericWindow::onUpdate( Subject<VarBool, void*> &rVariable, void*arg )
{
    if( m_varVisible.get() )
    {
        innerShow();
    }
    else
    {
        innerHide();
    }
}


void GenericWindow::innerShow()
{
    if( m_pOsWindow )
    {
        m_pOsWindow->show( m_left, m_top );
    }
}


void GenericWindow::innerHide()
{
    if( m_pOsWindow )
    {
        m_pOsWindow->hide();
    }
}

