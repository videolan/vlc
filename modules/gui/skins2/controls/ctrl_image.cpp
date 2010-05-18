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
#include "../src/vlcproc.hpp"
#include "../src/scaled_bitmap.hpp"
#include "../src/art_bitmap.hpp"
#include "../utils/position.hpp"


CtrlImage::CtrlImage( intf_thread_t *pIntf, GenericBitmap &rBitmap,
                      CmdGeneric &rCommand, resize_t resizeMethod,
                      const UString &rHelp, VarBool *pVisible, bool art ):
    CtrlFlat( pIntf, rHelp, pVisible ),
    m_pBitmap( &rBitmap ), m_pOriginalBitmap( &rBitmap ),
    m_rCommand( rCommand ), m_resizeMethod( resizeMethod ), m_art( art )
{
    // Create an initial unscaled image in the buffer
    m_pImage = OSFactory::instance( pIntf )->createOSGraphics(
                                    rBitmap.getWidth(), rBitmap.getHeight() );
    m_pImage->drawBitmap( *m_pBitmap );

    // Observe the variable
    if( m_art )
    {
        VlcProc *pVlcProc = VlcProc::instance( getIntf() );
        pVlcProc->getStreamArtVar().addObserver( this );

        ArtBitmap::initArtBitmap( getIntf() );
    }

}


CtrlImage::~CtrlImage()
{
    delete m_pImage;

    if( m_art )
    {
        VlcProc *pVlcProc = VlcProc::instance( getIntf() );
        pVlcProc->getStreamArtVar().delObserver( this );

        ArtBitmap::freeArtBitmap( );
    }
}


void CtrlImage::handleEvent( EvtGeneric &rEvent )
{
    // No FSM for this simple transition
    if( rEvent.getAsString() == "mouse:right:up:none" )
    {
        CmdDlgShowPopupMenu( getIntf() ).execute();
    }
    else if( rEvent.getAsString() == "mouse:left:up:none" )
    {
        CmdDlgHidePopupMenu( getIntf() ).execute();
        CmdDlgHideVideoPopupMenu( getIntf() ).execute();
        CmdDlgHideAudioPopupMenu( getIntf() ).execute();
        CmdDlgHideMiscPopupMenu( getIntf() ).execute();
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
        x %= m_pImage->getWidth();
        y %= m_pImage->getHeight();
    }
    return m_pImage->hit( x, y );
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
            if( width > 0 && height > 0 )
            {
                if( width != m_pImage->getWidth() ||
                    height != m_pImage->getHeight() )
                {
                    OSFactory *pOsFactory = OSFactory::instance( getIntf() );
                    // Rescale the image with the actual size of the control
                    ScaledBitmap bmp( getIntf(), *m_pBitmap, width, height );
                    delete m_pImage;
                    m_pImage = pOsFactory->createOSGraphics( width, height );
                    m_pImage->drawBitmap( bmp, 0, 0 );
                }
                rImage.drawGraphics( *m_pImage, 0, 0, xDest, yDest );
            }
        }
        else if( m_resizeMethod == kMosaic )
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
        else if( m_resizeMethod == kScaleAndRatioPreserved )
        {
            int width = pPos->getWidth();
            int height = pPos->getHeight();

            int w = m_pBitmap->getWidth();
            int h = m_pBitmap->getHeight();

            int scaled_height = width * h / w;
            int scaled_width  = height * w / h;

            int x, y;

            if( scaled_height > height )
            {
                width = scaled_width;
                x = ( pPos->getWidth() - width ) / 2;
                y = 0;
            }
            else
            {
                height = scaled_height;
                x =  0;
                y = ( pPos->getHeight() - height ) / 2;
            }

            OSFactory *pOsFactory = OSFactory::instance( getIntf() );
            // Rescale the image with the actual size of the control
            ScaledBitmap bmp( getIntf(), *m_pBitmap, width, height );
            delete m_pImage;
            m_pImage = pOsFactory->createOSGraphics( width, height );
            m_pImage->drawBitmap( bmp, 0, 0 );

            rImage.drawGraphics( *m_pImage, 0, 0, xDest + x, yDest + y );
        }
    }
}

void CtrlImage::onUpdate( Subject<VarString> &rVariable, void* arg )
{
    VlcProc *pVlcProc = VlcProc::instance( getIntf() );

    if( &rVariable == &pVlcProc->getStreamArtVar() )
    {
        string str = ((VarString&)rVariable).get();
        GenericBitmap* pArt = (GenericBitmap*) ArtBitmap::getArtBitmap( str );

        m_pBitmap = pArt ? pArt : m_pOriginalBitmap;

        msg_Dbg( getIntf(), "art file %s to be displayed (wxh = %ix%i)",
                            str.c_str(),
                            m_pBitmap->getWidth(),
                            m_pBitmap->getHeight() );

        delete m_pImage;
        m_pImage = OSFactory::instance( getIntf() )->createOSGraphics(
                                        m_pBitmap->getWidth(),
                                        m_pBitmap->getHeight() );
        m_pImage->drawBitmap( *m_pBitmap );

        notifyLayout();
    }
}


