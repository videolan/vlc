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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "anim_bitmap.hpp"
#include "generic_bitmap.hpp"
#include "os_factory.hpp"
#include "os_graphics.hpp"
#include "os_timer.hpp"


AnimBitmap::AnimBitmap( intf_thread_t *pIntf, const GenericBitmap &rBitmap ):
    SkinObject( pIntf ),
    m_rBitmap( rBitmap ), m_pImage( rBitmap.getGraphics() ),
    m_nbFrames( rBitmap.getNbFrames() ), m_frameRate( rBitmap.getFrameRate() ),
    m_nbLoops( rBitmap.getNbLoops() ), m_curFrame( 0 ), m_curLoop( 0 ),
    m_pTimer( NULL ), m_cmdNextFrame( this )
{
    OSFactory *pOsFactory = OSFactory::instance( pIntf );

    // Create the timer
    m_pTimer = pOsFactory->createOSTimer( m_cmdNextFrame );
}


AnimBitmap::~AnimBitmap()
{
    delete m_pTimer;
}


void AnimBitmap::startAnim()
{
    if( m_nbFrames > 1 && m_frameRate > 0 )
        m_pTimer->start( 1000 / m_frameRate, false );
}


void AnimBitmap::stopAnim()
{
    m_pTimer->stop();
    m_curLoop = 0;
    m_curFrame = 0;
}


void AnimBitmap::draw( OSGraphics &rImage, int xDest, int yDest, int w, int h,
                       int xOffset, int yOffset )
{
    // Draw the current frame
    int height = m_pImage->getHeight() / m_nbFrames;
    int ySrc = height * m_curFrame;

    // The old way .... transparency was not taken care of
    // rImage.drawGraphics( *m_pImage, 0, ySrc, xDest, yDest,
    //                      m_pImage->getWidth(), height );

    rImage.drawBitmap( m_rBitmap,
                       xOffset, ySrc + yOffset,
                       xDest, yDest, w, h, true );
}


bool AnimBitmap::hit( int x, int y ) const
{
    int height = m_pImage->getHeight() / m_nbFrames;
    return y >= 0 && y < height &&
           m_pImage->hit( x, m_curFrame * height + y );
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

    if( m_pParent->m_nbLoops > 0 && m_pParent->m_curFrame == 0 )
    {
        m_pParent->m_curLoop += 1;

        if( m_pParent->m_curLoop == m_pParent->m_nbLoops )
        {
            m_pParent->stopAnim();
            m_pParent->m_curFrame = m_pParent->m_nbFrames - 1;
        }
    }

    // Notify the observer so that it can display the next frame
    m_pParent->notify();
}

