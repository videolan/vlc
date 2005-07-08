/*****************************************************************************
 * tooltip.cpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN (Centrale RÃ©seaux) and its contributors
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include "tooltip.hpp"
#include "generic_bitmap.hpp"
#include "generic_font.hpp"
#include "os_factory.hpp"
#include "os_graphics.hpp"
#include "os_tooltip.hpp"
#include "os_timer.hpp"
#include "var_manager.hpp"
#include "../utils/ustring.hpp"


Tooltip::Tooltip( intf_thread_t *pIntf, const GenericFont &rFont, int delay ):
    SkinObject( pIntf ), m_rFont( rFont ), m_delay( delay ), m_pImage( NULL ),
    m_xPos( -1 ), m_yPos( -1 )
{
    OSFactory *pOsFactory = OSFactory::instance( pIntf );
    m_pTimer = pOsFactory->createOSTimer( Callback( this, &doShow ) );
    m_pOsTooltip = pOsFactory->createOSTooltip();

    // Observe the tooltip text variable
    VarManager::instance( pIntf )->getTooltipText().addObserver( this );
}


Tooltip::~Tooltip()
{
    VarManager::instance( getIntf() )->getTooltipText().delObserver( this );
    SKINS_DELETE( m_pTimer );
    SKINS_DELETE( m_pOsTooltip );
    if( m_pImage )
    {
        delete m_pImage;
    }
}


void Tooltip::show()
{
    // (Re)start the timer
    m_pTimer->start( m_delay, true );
}


void Tooltip::hide()
{
    m_pTimer->stop();
    m_pOsTooltip->hide();
    m_xPos = -1;
    m_yPos = -1;
}


void Tooltip::onUpdate( Subject<VarText> &rVariable )
{
    // Redisplay the tooltip
    displayText( ((VarText&)rVariable).get() );
}


void Tooltip::displayText( const UString &rText )
{
    // Rebuild the image
    makeImage( rText );

    // Redraw the window if it is already visible
    if( m_xPos != -1 )
    {
        m_pOsTooltip->show( m_xPos, m_yPos, *m_pImage );
    }
}


void Tooltip::makeImage( const UString &rText )
{
    // Render the text on a bitmap
    GenericBitmap *pBmpTip = m_rFont.drawString( rText, 0 );
    if( !pBmpTip )
    {
        return;
    }
    int w = pBmpTip->getWidth() + 10;
    int h = m_rFont.getSize() + 8;

    // Create the image of the tooltip
    if( m_pImage )
    {
        delete m_pImage;
    }
    m_pImage = OSFactory::instance( getIntf() )->createOSGraphics( w, h );
    m_pImage->fillRect( 0, 0, w, h, 0xffffd0 );
    m_pImage->drawRect( 0, 0, w, h, 0x000000 );
    m_pImage->drawBitmap( *pBmpTip, 0, 0, 5, 5, -1, -1, true );

    delete pBmpTip;
}


void Tooltip::doShow( SkinObject *pObj )
{
    Tooltip *pThis = (Tooltip*)pObj;

    if( pThis->m_pImage )
    {
        if( pThis->m_xPos == -1 )
        {
            // Get the mouse coordinates and the image size
            OSFactory *pOsFactory = OSFactory::instance( pThis->getIntf() );
            int x, y;
            pOsFactory->getMousePos( x, y );
            int scrWidth = pOsFactory->getScreenWidth();
            int scrHeight = pOsFactory->getScreenHeight();
            int w = pThis->m_pImage->getWidth();
            int h = pThis->m_pImage->getHeight();

            // Compute the position of the tooltip
            x -= (w / 2 + 4);
            y += (h + 5);
            if( x + w > scrWidth )
                x -= (x + w - scrWidth);
            else if( x < 0 )
                x = 0;
            if( y + h > scrHeight )
                y -= (2 * h + 20);

            pThis->m_xPos = x;
            pThis->m_yPos = y;
        }

        // Show the tooltip window
        pThis->m_pOsTooltip->show( pThis->m_xPos, pThis->m_yPos,
                                   *(pThis->m_pImage) );
    }
}

