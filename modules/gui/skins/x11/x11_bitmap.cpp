/*****************************************************************************
 * x11_bitmap.cpp: X11 implementation of the Bitmap class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: x11_bitmap.cpp,v 1.1 2003/04/28 14:32:57 asmax Exp $
 *
 * Authors: Cyril Deguet     <asmax@videolan.org>
 *          Emmanuel Puig    <karibu@via.ecp.fr>
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


//---------------------------------------------------------------------------
//   X11Bitmap
//---------------------------------------------------------------------------
X11Bitmap::X11Bitmap( intf_thread_t *p_intf, string FileName, int AColor )
    : Bitmap( p_intf, FileName, AColor )
{
    // Find the display
    display = p_intf->p_sys->display;
    Window root = DefaultRootWindow( display );

    AlphaColor = AColor;

    // Load the bitmap image
    int hotspotX, hotspotY;
    int rc = XReadBitmapFile( display, root, FileName.c_str(),
                              (unsigned int*)&Width, (unsigned int*)&Height, 
                              &Bmp, &hotspotX, &hotspotY );
    if( rc != BitmapSuccess )
    {
        if( FileName != "" )
            msg_Warn( p_intf, "Couldn't load bitmap: %s", FileName.c_str() );
        Width = 0;
        Height = 0;
    }
/*    else
    {    
        Width = gdk_pixbuf_get_width( Bmp );
        Height = gdk_pixbuf_get_height( Bmp );

        if( AColor != 0 )
        {
            // Change black pixels to another color to avoid transparency
            int rowstride = gdk_pixbuf_get_rowstride( Bmp );
            guchar *pixel = gdk_pixbuf_get_pixels( Bmp ); 
            int pix_size = ( gdk_pixbuf_get_has_alpha( Bmp ) ? 4 : 3 );
            
            for( int y = 0; y < Height; y++ )
            {
                for( int x = 0; x < Width; x++ )
                {   
                    guint32 r = pixel[0];
                    guint32 g = pixel[1]<<8;
                    guint32 b = pixel[2]<<16;
                    if( r+g+b == 0 )
                    {
                        pixel[2] = 10; // slight blue
                    }
                    pixel += pix_size;
                }
            }
        }

        Bmp = gdk_pixbuf_add_alpha( Bmp, TRUE, AColor & 0xff, (AColor>>8) & 0xff, 
                (AColor>>16) & 0xff );
    }*/
}
//---------------------------------------------------------------------------
X11Bitmap::X11Bitmap( intf_thread_t *p_intf, Graphics *from, int x, int y,
    int w, int h, int AColor ) : Bitmap( p_intf, from, x, y, w, h, AColor )
{
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
X11Bitmap::X11Bitmap( intf_thread_t *p_intf, Bitmap *c )
    : Bitmap( p_intf, c )
{
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
    XFreePixmap( display, Bmp );
}
//---------------------------------------------------------------------------
void X11Bitmap::DrawBitmap( int x, int y, int w, int h, int xRef, int yRef,
                              Graphics *dest )
{
    if( Bmp )
    {
/*        GdkDrawable *destImg = ( (X11Graphics *)dest )->GetImage();
        GdkGC *destGC = ( (X11Graphics *)dest )->GetGC();

        gdk_pixbuf_render_to_drawable( Bmp, destImg, destGC, x, y, xRef, yRef, 
                w, h, GDK_RGB_DITHER_NORMAL, 0, 0);*/
    }
}
//---------------------------------------------------------------------------
bool X11Bitmap::Hit( int x, int y)
{
//    unsigned int c = (unsigned int)GetBmpPixel( x, y );

/*    if( c == -1 || c == AlphaColor )
        return false;
    else
        return true;*/
}
//---------------------------------------------------------------------------
int X11Bitmap::GetBmpPixel( int x, int y )
{
    if( !Bmp || x < 0 || x >= Width || y < 0 || y >= Height )
        return -1;

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
