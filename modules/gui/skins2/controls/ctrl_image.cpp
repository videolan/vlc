/*****************************************************************************
 * ctrl_image.cpp
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

#include "ctrl_image.hpp"
#include "../commands/cmd_dialogs.hpp"
#include "../events/evt_generic.hpp"
#include "../src/os_factory.hpp"
#include "../src/os_graphics.hpp"
#include "../src/scaled_bitmap.hpp"
#include "../utils/position.hpp"


CtrlImage::CtrlImage( intf_thread_t *pIntf, const GenericBitmap &rBitmap,
                      CmdGeneric &rCommand, resize_t resizeMethod,
                      const UString &rHelp, VarBool *pVisible ):
    CtrlFlat( pIntf, rHelp, pVisible ), m_rBitmap( rBitmap ),
    m_rCommand( rCommand ), m_resizeMethod( resizeMethod )
{
    OSFactory *pOsFactory = OSFactory::instance( pIntf );
    // Create an initial unscaled image in the buffer
    m_pImage = pOsFactory->createOSGraphics( rBitmap.getWidth(),
                                             rBitmap.getHeight() );
    m_pImage->drawBitmap( m_rBitmap );
}


CtrlImage::~CtrlImage()
{
    SKINS_DELETE( m_pImage );
}


void CtrlImage::handleEvent( EvtGeneric &rEvent )
{
    // No FSM for this simple transition
    if( rEvent.getAsString() == "mouse:right:up:none" )
    {
        CmdDlgShowPopupMenu cmd( getIntf() );
        cmd.execute();
    }
    else if( rEvent.getAsString() == "mouse:left:up:none" )
    {
        CmdDlgHidePopupMenu cmd( getIntf() );
        cmd.execute();
    }
    else if( rEvent.getAsString() == "mouse:left:dblclick:none" )
    {
        m_rCommand.execute();
    }
}


bool CtrlImage::mouseOver( int x, int y ) const
{
    if( m_resizeMethod == kMosaic &&
        x >= 0 && x < getPosition()->getWidth() &&
        y >= 0 && y < getPosition()->getHeight() )
    {
        // In mosaic mode, convert the coordinates to make them fit to the
        // size of the original image
        return m_pImage->hit( x % m_pImage->getWidth(),
                              y % m_pImage->getHeight() );
    }
    else
    {
        return m_pImage->hit( x, y );
    }
}


void CtrlImage::draw( OSGraphics &rImage, int xDest, int yDest )
{
    const Position *pPos = getPosition();
    if( pPos )
    {
        int width = pPos->getWidth();
        int height = pPos->getHeight();

        if( m_resizeMethod == kScale )
        {
            // Use scaling method
            if( width != m_pImage->getWidth() ||
                height != m_pImage->getHeight() )
            {
                OSFactory *pOsFactory = OSFactory::instance( getIntf() );
                // Rescale the image with the actual size of the control
                ScaledBitmap bmp( getIntf(), m_rBitmap, width, height );
                SKINS_DELETE( m_pImage );
                m_pImage = pOsFactory->createOSGraphics( width, height );
                m_pImage->drawBitmap( bmp, 0, 0 );
            }
            rImage.drawGraphics( *m_pImage, 0, 0, xDest, yDest );
        }
        else
        {
            // Use mosaic method
            while( width > 0 )
            {
                int curWidth = __MIN( width, m_pImage->getWidth() );
                height = pPos->getHeight();
                int curYDest = yDest;
                while( height > 0 )
                {
                    int curHeight = __MIN( height, m_pImage->getHeight() );
                    rImage.drawGraphics( *m_pImage, 0, 0, xDest, curYDest,
                                         curWidth, curHeight );
                    curYDest += curHeight;
                    height -= m_pImage->getHeight();
                }
                xDest += curWidth;
                width -= m_pImage->getWidth();
            }
        }
    }
}

