/*****************************************************************************
 * x11_display.cpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
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

#ifdef X11_SKINS

#include <X11/Xutil.h>
#include <X11/extensions/shape.h>

#include "x11_display.hpp"
#include "../src/logger.hpp"


X11Display::X11Display( intf_thread_t *pIntf ): SkinObject( pIntf ),
    m_gc( NULL ), m_colormap( 0 )
{
    // Open a connection to the X Server
    m_pDisplay = XOpenDisplay( NULL );

    if( m_pDisplay == NULL )
    {
        MSG_ERR( "Cannot open display" );
        return;
    }

    // Load the XShape extension
    int event, error;
    XShapeQueryExtension( m_pDisplay, &event, &error );

    // Get the display parameters
    int screen = DefaultScreen( m_pDisplay );
    int depth = DefaultDepth( m_pDisplay, screen );
    int order = ImageByteOrder( m_pDisplay );

    // Template for looking up the XVisualInfo
    XVisualInfo xVInfoTemplate;
    xVInfoTemplate.screen = screen;
    xVInfoTemplate.depth = depth;

    XVisualInfo *pVInfo = NULL;
    int vCount = 0;

    switch( depth )
    {
        case 8:
            xVInfoTemplate.c_class = DirectColor;
            // Get the DirectColor visual
            pVInfo = XGetVisualInfo( m_pDisplay, VisualScreenMask |
                                     VisualClassMask, &xVInfoTemplate,
                                     &vCount );
            if( pVInfo == NULL )
            {
                msg_Err( getIntf(), "no DirectColor visual available" );
                m_pDisplay = NULL;
                break;
            }
            m_pVisual = pVInfo->visual;

            // Compute the color shifts
            getShifts( pVInfo->red_mask, m_redLeftShift, m_redRightShift );
            getShifts( pVInfo->green_mask, m_greenLeftShift,
                       m_greenRightShift );
            getShifts( pVInfo->blue_mask, m_blueLeftShift, m_blueRightShift );

            // Create a color map
            m_colormap = XCreateColormap( m_pDisplay,
                    DefaultRootWindow( m_pDisplay ),
                    DefaultVisual( m_pDisplay, screen ), AllocAll );

            // Create the palette
            XColor pColors[255];
            for( uint16_t i = 0; i < 255; i++ )
            {
                // kludge: colors are indexed reversely because color 255 seems
                // to bereserved for black even if we try to set it to white
                pColors[i].pixel = 254-i;
                pColors[i].pad   = 0;
                pColors[i].flags = DoRed | DoGreen | DoBlue;
                pColors[i].red   = (i >> m_redLeftShift) << (m_redRightShift + 8);
                pColors[i].green = (i >> m_greenLeftShift) << (m_greenRightShift + 8);
                pColors[i].blue  = (i >> m_blueLeftShift) << (m_blueRightShift + 8);
            }
            XStoreColors( m_pDisplay, m_colormap, pColors, 255 );
            makePixelImpl = &X11Display::makePixel8;
            m_pixelSize = 1;
            break;

        case 16:
        case 24:
        case 32:
            // Get the TrueColor visual
            xVInfoTemplate.c_class = TrueColor;
            pVInfo = XGetVisualInfo( m_pDisplay, VisualScreenMask |
                                     VisualDepthMask | VisualClassMask,
                                     &xVInfoTemplate, &vCount );
            if( pVInfo == NULL )
            {
                msg_Err( getIntf(), "No TrueColor visual for depth %d",
                         depth );
                m_pDisplay = NULL;
                break;
            }
            m_pVisual = pVInfo->visual;

            // Compute the color shifts
            getShifts( pVInfo->red_mask, m_redLeftShift, m_redRightShift );
            getShifts( pVInfo->green_mask, m_greenLeftShift,
                       m_greenRightShift );
            getShifts( pVInfo->blue_mask, m_blueLeftShift, m_blueRightShift );

            if( depth == 8 )
            {
                makePixelImpl = &X11Display::makePixel8;
                m_pixelSize = 1;
            }

            if( depth == 16 )
            {
                if( order == MSBFirst )
                {
                    makePixelImpl = &X11Display::makePixel16MSB;
                }
                else
                {
                    makePixelImpl = &X11Display::makePixel16LSB;
                }
                m_pixelSize = 2;
            }
            else
            {
                if( order == MSBFirst )
                {
                    makePixelImpl = &X11Display::makePixel32MSB;
                }
                else
                {
                    makePixelImpl = &X11Display::makePixel32LSB;
                }
                m_pixelSize = 4;
            }
            break;

        default:
            msg_Err( getIntf(), "Unsupported depth: %d bpp\n", depth );
            m_pDisplay = NULL;
            break;
    }

    // Free the visual info
    if( pVInfo )
    {
        XFree( pVInfo );
    }

    // Create a graphics context that doesn't generate GraphicsExpose events
    if( m_pDisplay )
    {
        XGCValues xgcvalues;
        xgcvalues.graphics_exposures = False;
        m_gc = XCreateGC( m_pDisplay, DefaultRootWindow( m_pDisplay ),
                          GCGraphicsExposures, &xgcvalues );
    }
}


X11Display::~X11Display()
{
    if( m_gc )
    {
        XFreeGC( m_pDisplay, m_gc );
    }
    if( m_colormap )
    {
        XFreeColormap( m_pDisplay, m_colormap );
    }
    if( m_pDisplay )
    {
        XCloseDisplay( m_pDisplay );
    }
}


void X11Display::getShifts( uint32_t mask, int &rLeftShift,
                            int &rRightShift ) const
{
    for( rLeftShift = 0; (rLeftShift < 32) && !(mask & 1); rLeftShift++ )
    {
        mask >>= 1;
    }
    for( rRightShift = 8; (mask & 1) ; rRightShift--)
    {
        mask >>= 1;
    }
    if( rRightShift < 0 )
    {
        rLeftShift -= rRightShift;
        rRightShift = 0;
    }
}


void X11Display::makePixel8( uint8_t *pPixel, uint8_t r, uint8_t g, uint8_t b,
                             uint8_t a ) const
{
    // Get the current pixel value
    uint8_t value = 255 - *pPixel;

    // Compute the new color values
    uint16_t temp;
    temp = ((uint8_t)((value >> m_redLeftShift) << m_redRightShift));
    uint8_t red = ( temp * (255 - a) + r * a ) / 255;
    temp = ((uint8_t)((value >> m_greenLeftShift) << m_greenRightShift));
    uint8_t green = ( temp * (255 - a) + g * a ) / 255;
    temp = ((uint8_t)((value >> m_blueLeftShift) << m_blueRightShift));
    uint8_t blue = ( temp * (255 - a) + b * a ) / 255;

    // Set the new pixel value
    value =
        ( ((uint8_t)red >> m_redRightShift) << m_redLeftShift ) |
        ( ((uint8_t)green >> m_greenRightShift) << m_greenLeftShift ) |
        ( ((uint8_t)blue >> m_blueRightShift) << m_blueLeftShift );

    *pPixel = 255 - value;
}


void X11Display::makePixel16MSB( uint8_t *pPixel, uint8_t r, uint8_t g,
                                 uint8_t b, uint8_t a ) const
{
    // Get the current pixel value
    uint16_t value = pPixel[1] | pPixel[0] << 8;

    // Compute the new color values
    uint16_t temp;
    temp = ((uint8_t)((value >> m_redLeftShift) << m_redRightShift));
    uint8_t red = ( temp * (255 - a) + r * a ) / 255;
    temp = ((uint8_t)((value >> m_greenLeftShift) << m_greenRightShift));
    uint8_t green = ( temp * (255 - a) + g * a ) / 255;
    temp = ((uint8_t)((value >> m_blueLeftShift) << m_blueRightShift));
    uint8_t blue = ( temp * (255 - a) + b * a ) / 255;

    // Set the new pixel value
    value =
        ( ((uint16_t)red >> m_redRightShift) << m_redLeftShift ) |
        ( ((uint16_t)green >> m_greenRightShift) << m_greenLeftShift ) |
        ( ((uint16_t)blue >> m_blueRightShift) << m_blueLeftShift );

    pPixel[1] = value;
    value >>= 8;
    pPixel[0] = value;
}


void X11Display::makePixel16LSB( uint8_t *pPixel, uint8_t r, uint8_t g,
                                 uint8_t b, uint8_t a ) const
{
    // Get the current pixel value
    uint16_t value = pPixel[0] | pPixel[1] << 8;

    // Compute the new color values
    uint16_t temp;
    temp = ((uint8_t)((value >> m_redLeftShift) << m_redRightShift));
    uint8_t red = ( temp * (255 - a) + r * a ) / 255;
    temp = ((uint8_t)((value >> m_greenLeftShift) << m_greenRightShift));
    uint8_t green = ( temp * (255 - a) + g * a ) / 255;
    temp = ((uint8_t)((value >> m_blueLeftShift) << m_blueRightShift));
    uint8_t blue = ( temp * (255 - a) + b * a ) / 255;

    // Set the new pixel value
    value =
        ( ((uint16_t)red >> m_redRightShift) << m_redLeftShift ) |
        ( ((uint16_t)green >> m_greenRightShift) << m_greenLeftShift ) |
        ( ((uint16_t)blue >> m_blueRightShift) << m_blueLeftShift );

    pPixel[0] = value;
    value >>= 8;
    pPixel[1] = value;
}


void X11Display::makePixel32MSB( uint8_t *pPixel, uint8_t r, uint8_t g,
                                 uint8_t b, uint8_t a ) const
{
    // Get the current pixel value
    uint32_t value = pPixel[3] | pPixel[2] << 8 | pPixel[1] << 16 |
                          pPixel[0] << 24;

    // Compute the new color values
    uint16_t temp;
    temp = ((uint8_t)((value >> m_redLeftShift) << m_redRightShift));
    uint8_t red = ( temp * (255 - a) + r * a ) / 255;
    temp = ((uint8_t)((value >> m_greenLeftShift) << m_greenRightShift));
    uint8_t green = ( temp * (255 - a) + g * a ) / 255;
    temp = ((uint8_t)((value >> m_blueLeftShift) << m_blueRightShift));
    uint8_t blue = ( temp * (255 - a) + b * a ) / 255;

    // Set the new pixel value
    value =
        ( ((uint32_t)red >> m_redRightShift) << m_redLeftShift ) |
        ( ((uint32_t)green >> m_greenRightShift) << m_greenLeftShift ) |
        ( ((uint32_t)blue >> m_blueRightShift) << m_blueLeftShift );

    pPixel[3] = value;
    value >>= 8;
    pPixel[2] = value;
    value >>= 8;
    pPixel[1] = value;
    value >>= 8;
    pPixel[0] = value;
}


void X11Display::makePixel32LSB( uint8_t *pPixel, uint8_t r, uint8_t g,
                                 uint8_t b, uint8_t a ) const
{
    // Get the current pixel value
    uint32_t value = pPixel[0] | pPixel[1] << 8 | pPixel[2] << 16 |
                          pPixel[3] << 24;

    // Compute the new color values
    uint16_t temp;
    temp = ((uint8_t)((value >> m_redLeftShift) << m_redRightShift));
    uint8_t red = ( temp * (255 - a) + r * a ) / 255;
    temp = ((uint8_t)((value >> m_greenLeftShift) << m_greenRightShift));
    uint8_t green = ( temp * (255 - a) + g * a ) / 255;
    temp = ((uint8_t)((value >> m_blueLeftShift) << m_blueRightShift));
    uint8_t blue = ( temp * (255 - a) + b * a ) / 255;

    // Set the new pixel value
    value =
        ( ((uint32_t)red >> m_redRightShift) << m_redLeftShift ) |
        ( ((uint32_t)green >> m_greenRightShift) << m_greenLeftShift ) |
        ( ((uint32_t)blue >> m_blueRightShift) << m_blueLeftShift );

    pPixel[0] = value;
    value >>= 8;
    pPixel[1] = value;
    value >>= 8;
    pPixel[2] = value;
    value >>= 8;
    pPixel[3] = value;
}


unsigned long X11Display::getPixelValue( uint8_t r, uint8_t g, uint8_t b ) const
{
    unsigned long value;
    value = ( ((uint32_t)r >> m_redRightShift) << m_redLeftShift ) |
            ( ((uint32_t)g >> m_greenRightShift) << m_greenLeftShift ) |
            ( ((uint32_t)b >> m_blueRightShift) << m_blueLeftShift );
    if( m_pixelSize == 1 )
    {
        return 255 - value;
    }
    else
    {
        return value;
    }
}


#endif
