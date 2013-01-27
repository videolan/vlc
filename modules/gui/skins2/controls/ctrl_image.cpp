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
#include "../src/art_manager.hpp"
#include "../utils/position.hpp"


CtrlImage::CtrlImage( intf_thread_t *pIntf, GenericBitmap &rBitmap,
                      CmdGeneric &rCommand, resize_t resizeMethod,
                      const UString &rHelp, VarBool *pVisible, bool art ):
    CtrlFlat( pIntf, rHelp, pVisible ),
    m_pBitmap( &rBitmap ), m_pOriginalBitmap( &rBitmap ),
    m_rCommand( rCommand ), m_resizeMethod( resizeMethod ), m_art( art ),
    m_x( 0 ), m_y( 0 )
{
    if( m_art )
    {
        // art file if any will overwrite the original image
        VlcProc *pVlcProc = VlcProc::instance( getIntf() );
        ArtManager* pArtManager = ArtManager::instance( getIntf() );

        // add observer
        pVlcProc->getStreamArtVar().addObserver( this );

        // retrieve initial state of art file
        string str = pVlcProc->getStreamArtVar().get();
        GenericBitmap* pArt = (GenericBitmap*) pArtManager->getArtBitmap( str );
        if( pArt )
        {
            m_pBitmap = pArt;
            msg_Dbg( getIntf(), "art file %s to be displayed (wxh = %ix%i)",
                str.c_str(), m_pBitmap->getWidth(), m_pBitmap->getHeight() );
        }
    }

    // Create the initial image
    m_pImage = OSFactory::instance( getIntf() )->createOSGraphics(
                                    m_pBitmap->getWidth(),
                                    m_pBitmap->getHeight() );
    m_pImage->drawBitmap( *m_pBitmap );
}


CtrlImage::~CtrlImage()
{
    delete m_pImage;

    if( m_art )
    {
        VlcProc *pVlcProc = VlcProc::instance( getIntf() );
        pVlcProc->getStreamArtVar().delObserver( this );
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
    if( x >= 0 && x < getPosition()->getWidth() &&
        y >= 0 && y < getPosition()->getHeight() )
    {
        // convert the coordinates to make them fit to the
        // size of the original image if needed
        switch( m_resizeMethod )
        {
        case kMosaic:
            x %= m_pImage->getWidth();
            y %= m_pImage->getHeight();
            break;

        case kScaleAndRatioPreserved:
            x -= m_x;
            y -= m_y;
            break;

        case kScale:
            break;
        }
        return m_pImage->hit( x, y );
    }

    return false;
}


void CtrlImage::draw( OSGraphics &rImage, int xDest, int yDest, int w, int h )
{
    const Position *pPos = getPosition();
    if( !pPos )
        return;

    int width = pPos->getWidth();
    int height = pPos->getHeight();
    if( width <= 0 || height <= 0 )
        return;

    rect region( pPos->getLeft(), pPos->getTop(),
                 pPos->getWidth(), pPos->getHeight() );
    rect clip( xDest, yDest, w, h );
    rect inter;
    if( !rect::intersect( region, clip, &inter ) )
        return;

    if( m_resizeMethod == kScale )
    {
        // Use scaling method
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
        rImage.drawGraphics( *m_pImage,
                             inter.x - pPos->getLeft(),
                             inter.y - pPos->getTop(),
                             inter.x, inter.y,
                             inter.width, inter.height );
    }
    else if( m_resizeMethod == kMosaic )
    {
        int xDest0 = pPos->getLeft();
        int yDest0 = pPos->getTop();

        // Use mosaic method
        while( width > 0 )
        {
            int curWidth = __MIN( width, m_pImage->getWidth() );
            height = pPos->getHeight();
            int curYDest = yDest0;
            while( height > 0 )
            {
                int curHeight = __MIN( height, m_pImage->getHeight() );
                rect region1( xDest0, curYDest, curWidth, curHeight );
                rect inter1;
                if( rect::intersect( region1, clip, &inter1 ) )
                {
                    rImage.drawGraphics( *m_pImage,
                                   inter1.x - region1.x,
                                   inter1.y - region1.y,
                                   inter1.x, inter1.y,
                                   inter1.width, inter1.height );
                }
                curYDest += curHeight;
                height -= m_pImage->getHeight();
            }
            xDest0 += curWidth;
            width -= m_pImage->getWidth();
        }
    }
    else if( m_resizeMethod == kScaleAndRatioPreserved )
    {
        int w0 = m_pBitmap->getWidth();
        int h0 = m_pBitmap->getHeight();

        int scaled_height = width * h0 / w0;
        int scaled_width  = height * w0 / h0;

        // new image scaled with aspect ratio preserved
        // and centered inside the control boundaries
        int w, h;
        if( scaled_height > height )
        {
            w = scaled_width;
            h = height;
            m_x = ( width - w ) / 2;
            m_y = 0;
        }
        else
        {
            w = width;
            h = scaled_height;
            m_x = 0;
            m_y = ( height - h ) / 2;
        }

        // rescale the image if size changed
        if( w != m_pImage->getWidth() ||
            h != m_pImage->getHeight() )
        {
            OSFactory *pOsFactory = OSFactory::instance( getIntf() );
            ScaledBitmap bmp( getIntf(), *m_pBitmap, w, h );
            delete m_pImage;
            m_pImage = pOsFactory->createOSGraphics( w, h );
            m_pImage->drawBitmap( bmp, 0, 0 );
        }

        // draw the scaled image at offset (m_x, m_y) from control origin
        rect region1( pPos->getLeft() + m_x, pPos->getTop() + m_y, w, h );
        rect inter1;
        if( rect::intersect( region1, inter, &inter1 ) )
        {
            rImage.drawGraphics( *m_pImage,
                                 inter1.x - pPos->getLeft() - m_x,
                                 inter1.y - pPos->getTop() - m_y,
                                 inter1.x, inter1.y,
                                 inter1.width, inter1.height );
        }
    }
}


void CtrlImage::onUpdate( Subject<VarString> &rVariable, void* arg )
{
    (void)arg;
    VlcProc *pVlcProc = VlcProc::instance( getIntf() );

    if( &rVariable == &pVlcProc->getStreamArtVar() )
    {
        string str = ((VarString&)rVariable).get();
        ArtManager* pArtManager = ArtManager::instance( getIntf() );
        GenericBitmap* pArt = (GenericBitmap*) pArtManager->getArtBitmap( str );

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


