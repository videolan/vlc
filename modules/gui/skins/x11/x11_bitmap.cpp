/*****************************************************************************
 * x11_bitmap.cpp: X11 implementation of the Bitmap class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: x11_bitmap.cpp,v 1.3 2003/05/18 11:25:00 asmax Exp $
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
    char *data = NULL;
    Width = 0;
    Height = 0;

    AlphaColor = AColor;

    if( FileName != "" )
    {
        data = LoadFromFile( FileName, depth, Width, Height );
    }

    // Create the image
    Bmp = XCreateImage( display, visual, depth, ZPixmap, 0, data, Width, 
                        Height, 32, 4 * Width );
    XInitImage( Bmp );
    
    // Load the bitmap image
/*    if( rc != BitmapSuccess )
    {
        if( FileName != "" )
            msg_Warn( p_intf, "Couldn't load bitmap: %s", FileName.c_str() );
        Width = 0;
        Height = 0;
    }*/
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
    XDestroyImage( Bmp );
}
//---------------------------------------------------------------------------
void X11Bitmap::DrawBitmap( int x, int y, int w, int h, int xRef, int yRef,
                              Graphics *dest )
{
    Drawable destImg = ( (X11Graphics *)dest )->GetImage();
    GC destGC = ( (X11Graphics *)dest )->GetGC();
    XPutImage( display, destImg, destGC, Bmp, x, y, xRef, yRef, w, h );
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
char *X11Bitmap::LoadFromFile( string fileName, int depth, int &width, int &height )
{
    // BMP header fields
    uint32_t fileSize;
    uint32_t dataOffset;
    uint32_t headerSize;
   	uint16_t planes;
   	uint16_t bpp;
   	uint32_t compression;
   	uint32_t dataSize;
   	uint32_t nColors;
    
    FILE *file = fopen( fileName.c_str(), "ro" );
    if( !file )
    {
        msg_Warn( p_intf, "Cannot open %s", fileName.c_str() );
        return NULL;
    }

    // Read the headers
    char headers[54];
    fread( headers, 54, 1, file );
    
    fileSize = U32( headers + 2 );
    dataOffset = U32( headers + 10 );
    headerSize = U32( headers + 14 );
    width = (int32_t) U32( headers + 18 );
    height = (int32_t) U32( headers + 22 ); 
    planes = U32( headers + 26 );
    bpp = U32( headers + 28 );
    compression = U32( headers + 30 );
    dataSize = U32( headers + 34 );
    nColors = U32( headers + 50 );

//    fprintf(stderr,"image %d %d\n", width, height);
    switch( bpp )
    {
        case 24:
        // 24 bits per pixel
        {
            // Pad to a 32bit boundary
            int pad = ((3 * width - 1) / 4) * 4 + 4 - 3 * width;
            uint32_t *data = new uint32_t[height * width];
            uint32_t *ptr;
            for( int j = 0; j < height; j++ )
            {
                ptr = data +  width * (height - j - 1);
                for( int i = 0; i < width; i++ )
                {
                    // Read a pixel
                    uint32_t pixel = 0;
                    fread( &pixel, 3, 1, file );
                    *(ptr++) = U32( &pixel );
                }
                fseek( file, pad, SEEK_CUR );
            }
            return (char*)data;
        }
        default:
            msg_Warn( p_intf, "%s : %d bbp not supported !", fileName.c_str(), 
                      bpp );
            return NULL;
    }
    
}
//---------------------------------------------------------------------------

#endif
