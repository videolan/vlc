/*****************************************************************************
 * x11_bitmap.cpp: X11 implementation of the Bitmap class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: x11_bitmap.cpp,v 1.8 2003/06/01 16:39:49 asmax Exp $
 *
 * Authors: Cyril Deguet     <asmax@videolan.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111,
 * USA.
 *****************************************************************************/

#ifdef X11_SKINS

//--- X11 -------------------------------------------------------------------
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <Imlib2.h>

//--- VLC -------------------------------------------------------------------
#include <vlc/intf.h>

//--- SKIN ------------------------------------------------------------------
#include "../os_api.h"
#include "../src/graphics.h"
#include "x11_graphics.h"
#include "../src/bitmap.h"
#include "x11_bitmap.h"
#include "../src/theme.h"
#include "../os_theme.h"
#include "../src/skin_common.h"

#include <stdio.h>

// macros to read little endian numbers
#define U16( p ) ( ((uint8_t*)(p))[0] | ((uint8_t*)(p))[1] << 8 )
#define U32( p ) ( U16( p ) | ((uint8_t*)(p))[2] << 16 | ((uint8_t*)(p))[3] << 24 )

//---------------------------------------------------------------------------
//   X11Bitmap
//---------------------------------------------------------------------------
X11Bitmap::X11Bitmap( intf_thread_t *_p_intf, string FileName, int AColor )
    : Bitmap( p_intf, FileName, AColor )
{
    p_intf = _p_intf;
    
    // Find the display
    display = p_intf->p_sys->display;
    int screen = DefaultScreen( display );
    int depth = DefaultDepth( display, screen );
    Screen *screenptr = DefaultScreenOfDisplay( display );
    Visual *visual = DefaultVisualOfScreen( screenptr );
    Img = NULL;
    Width = 0;
    Height = 0;

    if( FileName == "" )
    {
        return;
    }
 
    AlphaColor = (AColor & 0xff) << 16 | (AColor & 0xff00) | 
                 (AColor & 0xff0000) >> 16;
                 
    imlib_context_set_display( display );
    imlib_context_set_visual( visual );
    imlib_context_set_colormap( DefaultColormap( display, screen ) );
    imlib_context_set_dither( 1 );
    imlib_context_set_blend( 1 );

    Img = imlib_load_image_immediately( FileName.c_str() );
    imlib_context_set_image( Img );
    Width = imlib_image_get_width();
    Height = imlib_image_get_height();
 
    // Add an alpha layer
    DATA32 *data = imlib_image_get_data();
    DATA32 *ptr = data;
    for( int j = 0; j < Height; j++)
    {
        for( int i = 0; i < Width; i++)
        {
            if( AlphaColor != 0 && *ptr == 0xff000000 )
            {
                // Avoid transparency for black pixels
                *ptr = 0xff00000c;
            }
            else if( (*ptr & 0xffffff) == AlphaColor )
            {
                *ptr &= 0x00ffffff;
            }
            ptr++;
        }
    }
    imlib_image_set_has_alpha( 1 );
    imlib_image_set_irrelevant_alpha( 0 );
    imlib_image_put_back_data( data );
}
//---------------------------------------------------------------------------
X11Bitmap::X11Bitmap( intf_thread_t *_p_intf, Graphics *from, int x, int y,
    int w, int h, int AColor ) : Bitmap( p_intf, from, x, y, w, h, AColor )
{
    p_intf = _p_intf;
/*    Width  = w;
    Height = h;
    AlphaColor = AColor;
    HBITMAP HBmp;
    HDC fromDC = ( (X11Graphics *)from )->GetImageHandle();

    // Create image
    bmpDC = CreateCompatibleDC( fromDC );
    HBmp  = CreateCompatibleBitmap( fromDC, Width, Height );
    SelectObject( bmpDC, HBmp );
    DeleteObject( HBmp );
    BitBlt( bmpDC, 0, 0, Width, Height, fromDC, x, y, SRCCOPY );*/
}
//---------------------------------------------------------------------------
X11Bitmap::X11Bitmap( intf_thread_t *_p_intf, Bitmap *c )
    : Bitmap( p_intf, c )
{
    p_intf = _p_intf;
/*    HBITMAP HBuf;

    // Copy attibutes
    c->GetSize( Width, Height );
    AlphaColor = c->GetAlphaColor();

    // Copy bmpDC
    bmpDC = CreateCompatibleDC( NULL );
    HBuf  = CreateCompatibleBitmap( bmpDC, Width, Height );
    SelectObject( bmpDC, HBuf );

    BitBlt( bmpDC, 0, 0, Width, Height, ( (X11Bitmap *)c )->GetBmpDC(),
            0, 0, SRCCOPY );
    DeleteObject( HBuf );*/
}
//---------------------------------------------------------------------------
X11Bitmap::~X11Bitmap()
{
    if( Img )
    {
        imlib_context_set_image( Img );
        imlib_free_image();
    }
}
//---------------------------------------------------------------------------
void X11Bitmap::DrawBitmap( int x, int y, int w, int h, int xRef, int yRef,
                              Graphics *dest )
{
    if( Img )
    {
        Drawable destImg = ( (X11Graphics *)dest )->GetImage();
        imlib_context_set_image( Img );
        imlib_context_set_drawable( destImg );
        imlib_render_image_part_on_drawable_at_size( x, y, w, h, xRef, yRef, w, h );
    }
}
//---------------------------------------------------------------------------
bool X11Bitmap::Hit( int x, int y)
{
    unsigned int c = (unsigned int)GetBmpPixel( x, y );

    if( c == -1 || c == AlphaColor )
        return false;
    else
        return true;
}
//---------------------------------------------------------------------------
int X11Bitmap::GetBmpPixel( int x, int y )
{
    if( !Img || x < 0 || x >= Width || y < 0 || y >= Height )
        return -1;

    return 42;
/*    guchar *pixels;
    int rowstride, offset;
    gboolean has_alpha;

    rowstride = gdk_pixbuf_get_rowstride( Bmp );
    pixels    = gdk_pixbuf_get_pixels( Bmp ); 
    has_alpha = gdk_pixbuf_get_has_alpha( Bmp );

    offset = y * rowstride + ( x * (has_alpha ? 4 : 3) );

    int r = pixels [offset];
    int g = pixels [offset + 1] << 8;
    int b = pixels [offset + 2] << 16;

    return r + g + b;*/
}
//---------------------------------------------------------------------------
void X11Bitmap::SetBmpPixel( int x, int y, int color )
{
//    SetPixelV( bmpDC, x, y, color );
}
//---------------------------------------------------------------------------

#endif
