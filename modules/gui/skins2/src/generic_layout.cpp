/*****************************************************************************
 * generic_layout.cpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: generic_layout.cpp,v 1.2 2004/02/29 16:49:55 asmax Exp $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include "generic_layout.hpp"
#include "generic_window.hpp"
#include "os_factory.hpp"
#include "os_graphics.hpp"
#include "../controls/ctrl_generic.hpp"


GenericLayout::GenericLayout( intf_thread_t *pIntf, int width, int height,
                              int minWidth, int maxWidth, int minHeight,
                              int maxHeight ):
    SkinObject( pIntf ), m_pWindow( NULL ), m_width( width ),
    m_height( height ), m_minWidth( minWidth ), m_maxWidth( maxWidth ),
    m_minHeight( minHeight ), m_maxHeight( maxHeight )
{
    // Get the OSFactory
    OSFactory *pOsFactory = OSFactory::instance( getIntf() );
    // Create the graphics buffer
    m_pImage = pOsFactory->createOSGraphics( width, height );
}


GenericLayout::~GenericLayout()
{
    if( m_pImage )
    {
        delete m_pImage;
    }
}


void GenericLayout::setWindow( GenericWindow *pWindow )
{
    m_pWindow = pWindow;
}


void GenericLayout::onControlCapture( const CtrlGeneric &rCtrl )
{
    // Just forward the request to the window
    GenericWindow *pWindow = getWindow();
    if( pWindow )
    {
        pWindow->onControlCapture( rCtrl );
    }
}


void GenericLayout::onControlRelease( const CtrlGeneric &rCtrl )
{
    // Just forward the request to the window
    GenericWindow *pWindow = getWindow();
    if( pWindow )
    {
        pWindow->onControlRelease( rCtrl );
    }
}


void GenericLayout::addControl( CtrlGeneric *pControl,
                              const Position &rPosition, int layer )
{
    if( pControl )
    {
        // Associate this layout to the control
        pControl->setLayout( this, rPosition );

        // Draw the control
        pControl->draw( *m_pImage, rPosition.getLeft(), rPosition.getTop() );

        // Add the control in the list.
        // This list must remain sorted by layer order 
        list<LayeredControl>::iterator it;
        for( it = m_controlList.begin(); it != m_controlList.end(); it++ )
        {
            if( layer < (*it).m_layer )
            {
                m_controlList.insert( it, LayeredControl( pControl, layer ) );
                break;
            }
        }
        // If this control is in front of all the previous ones
        if( it == m_controlList.end() )
        {
            m_controlList.push_back( LayeredControl( pControl, layer ) );
        }
    }
    else
    {
        msg_Dbg( getIntf(), "Adding NULL control in the layout" );
    }
}


const list<LayeredControl> &GenericLayout::getControlList() const
{
    return m_controlList;
}


void GenericLayout::onControlUpdate( const CtrlGeneric &rCtrl )
{
    // TODO: refresh only the needed area if possible
    refreshAll();
}


void GenericLayout::resize( int width, int height )
{
    if( width == m_width && height == m_height )
    {
        return;
    }

    // Update the window size
    m_width = width;
    m_height = height;

    // Recreate a new image
    if( m_pImage )
    {
        delete m_pImage;
        OSFactory *pOsFactory = OSFactory::instance( getIntf() );
        m_pImage = pOsFactory->createOSGraphics( width, height );
    }

    // Notify all the controls that the size has changed and redraw them
    list<LayeredControl>::const_iterator iter;
    for( iter = m_controlList.begin(); iter != m_controlList.end(); iter++ )
    {
        (*iter).m_pControl->onResize();
        const Position *pPos = (*iter).m_pControl->getPosition();
        if( pPos )
        {
            (*iter).m_pControl->draw( *m_pImage, pPos->getLeft(),
                                      pPos->getTop() );
        }
    }

    // Resize and refresh the associated window
    GenericWindow *pWindow = getWindow();
    if( pWindow )
    {
        // Resize the window
        pWindow->refresh( 0, 0, width, height );
        pWindow->resize( width, height );
        pWindow->refresh( 0, 0, width, height );

        // Change the shape of the window and redraw it
        pWindow->updateShape();
        pWindow->refresh( 0, 0, width, height );
    }
}


void GenericLayout::refreshAll()
{
    // Draw all the controls of the layout
    list<LayeredControl>::const_iterator iter;
    for( iter = m_controlList.begin(); iter != m_controlList.end(); iter++ )
    {
        CtrlGeneric *pCtrl = (*iter).m_pControl;
        const Position *pPos = pCtrl->getPosition();
        if( pCtrl->isVisible() && pPos )
        {
            pCtrl->draw( *m_pImage, pPos->getLeft(), pPos->getTop() );
        }
    }

    // Refresh the associated window
    GenericWindow *pWindow = getWindow();
    if( pWindow )
    {
        pWindow->refresh( 0, 0, m_width, m_height );
    }
}

