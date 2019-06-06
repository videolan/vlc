/*****************************************************************************
 * generic_window.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
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
#include "var_manager.hpp"
#include "../events/evt_refresh.hpp"


GenericWindow::GenericWindow( intf_thread_t *pIntf, int left, int top,
                              bool dragDrop, bool playOnDrop,
                              GenericWindow *pParent, WindowType_t type ):
    SkinObject( pIntf ), m_type( type ), m_left( left ), m_top( top ),
    m_width( 0 ), m_height( 0 ), m_pVarVisible( NULL )
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
                                              pOSParent, type );

    // Create the visibility variable and register it in the manager
    m_pVarVisible = new VarBoolImpl( pIntf );
    VarManager::instance( pIntf )->registerVar( VariablePtr( m_pVarVisible ) );

    // Observe the visibility variable
    m_pVarVisible->addObserver( this );
}


GenericWindow::~GenericWindow()
{
    m_pVarVisible->delObserver( this );

    delete m_pOsWindow;
}


void GenericWindow::processEvent( EvtRefresh &rEvtRefresh )
{
    // Refresh the given area
    refresh( rEvtRefresh.getXStart(), rEvtRefresh.getYStart(),
             rEvtRefresh.getWidth(), rEvtRefresh.getHeight() );
}


void GenericWindow::show() const
{
    m_pVarVisible->set( true );
}


void GenericWindow::hide() const
{
    m_pVarVisible->set( false );
}


void GenericWindow::move( int left, int top )
{
    // Update the window coordinates
    m_left = left;
    m_top = top;

    if( m_pOsWindow && isVisible() )
        m_pOsWindow->moveResize( left, top, m_width, m_height );
}


void GenericWindow::resize( int width, int height )
{
    // don't try when value is 0 (may crash)
    if( !width || !height )
        return;

    // Update the window size
    m_width = width;
    m_height = height;

    if( m_pOsWindow && isVisible() )
        m_pOsWindow->moveResize( m_left, m_top, width, height );
}


void GenericWindow::raise() const
{
    if( m_pOsWindow )
        m_pOsWindow->raise();
}


void GenericWindow::setOpacity( uint8_t value )
{
    m_pOsWindow->setOpacity( value );
}


void GenericWindow::toggleOnTop( bool onTop ) const
{
    if( m_pOsWindow )
        m_pOsWindow->toggleOnTop( onTop );
}


void GenericWindow::onUpdate( Subject<VarBool> &rVariable, void* arg )
{
    (void)rVariable; (void)arg;
    if (&rVariable == m_pVarVisible )
    {
        if( m_pVarVisible->get() )
        {
            innerShow();
        }
        else
        {
            innerHide();
        }
    }
}


void GenericWindow::innerShow()
{
    if( m_pOsWindow )
    {
        m_pOsWindow->show();
        m_pOsWindow->moveResize( m_left, m_top, m_width, m_height );
    }
}


void GenericWindow::innerHide()
{
    if( m_pOsWindow )
    {
        m_pOsWindow->hide();
    }
}


void GenericWindow::updateWindowConfiguration( vout_window_t * pWnd ) const
{
    m_pOsWindow->setOSHandle( pWnd );
}


void GenericWindow::setParent( GenericWindow* pParent, int x, int y, int w, int h )
{
    // Update the window size and position
    m_left = x;
    m_top = y;
    m_width  = ( w > 0 ) ? w : m_width;
    m_height = ( h > 0 ) ? h : m_height;

    OSWindow *pParentOSWindow = pParent->m_pOsWindow;
    m_pOsWindow->reparent( pParentOSWindow, m_left, m_top, m_width, m_height );
}


void GenericWindow::invalidateRect( int left, int top, int width, int height )
{
    if( m_pOsWindow )
    {
        // tell the OS we invalidate a window client area
        bool b_supported =
            m_pOsWindow->invalidateRect( left, top, width, height );

        // if not supported, directly refresh the area
        if( !b_supported )
            refresh( left, top, width, height );
    }
}


void GenericWindow::getMonitorInfo( int* x, int* y, int* width, int* height ) const
{
    OSFactory *pOsFactory = OSFactory::instance( getIntf() );
    pOsFactory->getMonitorInfo( m_pOsWindow, x, y, width, height );
}
