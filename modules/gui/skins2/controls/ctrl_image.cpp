/*****************************************************************************
 * ctrl_image.cpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: ctrl_image.cpp,v 1.1 2004/01/03 23:31:33 asmax Exp $
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

#include "ctrl_image.hpp"
#include "../commands/cmd_dialogs.hpp"
#include "../events/evt_generic.hpp"
#include "../src/os_factory.hpp"
#include "../src/os_graphics.hpp"
#include "../src/scaled_bitmap.hpp"
#include "../utils/position.hpp"


CtrlImage::CtrlImage( intf_thread_t *pIntf, const GenericBitmap &rBitmap,
                      const UString &rHelp ):
    CtrlFlat( pIntf, rHelp ), m_rBitmap( rBitmap )
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
    if( rEvent.getAsString() == "mouse:right:down:none" )
    {
        CmdDlgPopupMenu cmd( getIntf() );
        cmd.execute();
    }
}


bool CtrlImage::mouseOver( int x, int y ) const
{
    return m_pImage->hit( x, y );
}


void CtrlImage::draw( OSGraphics &rImage, int xDest, int yDest )
{
    const Position *pPos = getPosition();
    if( pPos )
    {
        int width = pPos->getWidth();
        int height = pPos->getHeight();
        if( width != m_pImage->getWidth() || height != m_pImage->getHeight() )
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
}

