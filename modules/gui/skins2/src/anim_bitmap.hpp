/*****************************************************************************
 * anim_bitmap.hpp
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef ANIM_BITMAP_HPP
#define ANIM_BITMAP_HPP

#include "../commands/cmd_generic.hpp"
#include "../utils/observer.hpp"
#include "../utils/position.hpp"

class GenericBitmap;
class OSGraphics;
class OSTimer;


/// Animated bitmap
class AnimBitmap: public SkinObject, public Box,
                  public Subject<AnimBitmap>
{
public:
    AnimBitmap( intf_thread_t *pIntf, const GenericBitmap &rBitmap );

    virtual ~AnimBitmap();

    /// Start the animation
    void startAnim();

    /// Stop the animation
    void stopAnim();

    /// Draw the current frame on another graphics
    void draw( OSGraphics &rImage, int xDest, int yDest, int w, int h, int xOffset = 0, int yOffset = 0 );

    /// Tell whether the pixel at the given position is visible
    bool hit( int x, int y ) const;

    /// Get the size of the current frame
    virtual int getWidth() const;
    virtual int getHeight() const;

    /// compare two animated image
    bool operator==( const AnimBitmap& other ) const;

private:
    /// Bitmap stored
    const GenericBitmap &m_rBitmap;
    /// Graphics to store the bitmap
    const OSGraphics * const m_pImage;
    /// Number of frames
    int m_nbFrames;
    /// Frame rate
    int m_frameRate;
    /// Number of Loops
    int m_nbLoops;
    /// Curent frame
    int m_curFrame;
    /// Current loop
    int m_curLoop;
     /// Timer for the animation
    OSTimer *m_pTimer;

    /// Callback for the timer
    DEFINE_CALLBACK( AnimBitmap, NextFrame );
};


#endif

