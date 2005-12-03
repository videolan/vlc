/*****************************************************************************
 * anim_bitmap.cpp
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include "anim_bitmap.hpp"
#include "generic_bitmap.hpp"
#include "os_factory.hpp"
#include "os_graphics.hpp"
#include "os_timer.hpp"


AnimBitmap::AnimBitmap( intf_thread_t *pIntf, const GenericBitmap &rBitmap ):
    SkinObject( pIntf ), m_pImage( NULL ), m_curFrame( 0 ), m_pTimer( NULL ),
    m_cmdNextFrame( this )
{
    // Build the graphics
    OSFactory *pOsFactory = OSFactory::instance( pIntf );
    m_pImage = pOsFactory->createOSGraphics( rBitmap.getWidth(),
                                             rBitmap.getHeight() );
    m_pImage->drawBitmap( rBitmap, 0, 0 );

    m_nbFrames = rBitmap.getNbFrames();
    m_frameRate = rBitmap.getFrameRate();

    // Create the timer
    m_pTimer = pOsFactory->createOSTimer( m_cmdNextFrame );
}


AnimBitmap::~AnimBitmap()
{
    delete m_pImage;
    delete m_pTimer;
}


void AnimBitmap::startAnim()
{
    if( m_nbFrames > 1 && m_frameRate > 0 )
    {
        m_pTimer->start( 1000 / m_frameRate, false );
    }
}


void AnimBitmap::stopAnim()
{
    m_pTimer->stop();
}


void AnimBitmap::draw( OSGraphics &rImage, int xDest, int yDest )
{
    // Draw the current frame
    int height = m_pImage->getHeight() / m_nbFrames;
    int ySrc = height * m_curFrame;
    rImage.drawGraphics( *m_pImage, 0, ySrc, xDest, yDest,
                         m_pImage->getWidth(), height );
}


bool AnimBitmap::hit( int x, int y ) const
{
    int height = m_pImage->getHeight() / m_nbFrames;
    if( y >= 0 && y < height )
    {
        return m_pImage->hit( x, m_curFrame * height + y );
    }
    else
    {
        return false;
    }
}


int AnimBitmap::getWidth() const
{
    return m_pImage->getWidth();
}


int AnimBitmap::getHeight() const
{
    return m_pImage->getHeight() / m_nbFrames;
}


void AnimBitmap::CmdNextFrame::execute()
{
    // Go the next frame
    m_pParent->m_curFrame = ( m_pParent->m_curFrame + 1 ) %
        m_pParent->m_nbFrames;

    // Notify the observer so that it can display the next frame
    m_pParent->notify();
}

