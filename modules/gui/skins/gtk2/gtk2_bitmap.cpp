/*****************************************************************************
 * gtk2_bitmap.cpp: GTK2 implementation of the Bitmap class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: gtk2_bitmap.cpp,v 1.17 2003/04/28 12:00:13 asmax Exp $
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

#ifdef GTK2_SKINS

//--- GTK2 -----------------------------------------------------------------
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>

//--- VLC -------------------------------------------------------------------
#include <vlc/intf.h>

//--- SKIN ------------------------------------------------------------------
#include "../os_api.h"
#include "../src/graphics.h"
#include "gtk2_graphics.h"
#include "../src/bitmap.h"
#include "gtk2_bitmap.h"
#include "../src/skin_common.h"


//---------------------------------------------------------------------------
//   GTK2Bitmap
//---------------------------------------------------------------------------
GTK2Bitmap::GTK2Bitmap( intf_thread_t *p_intf, string FileName, int AColor )
    : Bitmap( p_intf, FileName, AColor )
{
    AlphaColor = AColor;

    // Load the bitmap image
    Bmp = gdk_pixbuf_new_from_file( FileName.c_str(), NULL );
    if( Bmp == NULL )
    {
        if( FileName != "" )
            msg_Warn( p_intf, "Couldn't load bitmap: %s", FileName.c_str() );
        Width = 0;
        Height = 0;
    }
    else
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
    }
}
//---------------------------------------------------------------------------
GTK2Bitmap::GTK2Bitmap( intf_thread_t *p_intf, Graphics *from, int x, int y,
    int w, int h, int AColor ) : Bitmap( p_intf, from, x, y, w, h, AColor )
{
/*    Width  = w;
    Height = h;
    AlphaColor = AColor;
    HBITMAP HBmp;
    HDC fromDC = ( (GTK2Graphics *)from )->GetImageHandle();

    // Create image
    bmpDC = CreateCompatibleDC( fromDC );
    HBmp  = CreateCompatibleBitmap( fromDC, Width, Height );
    SelectObject( bmpDC, HBmp );
    DeleteObject( HBmp );
    BitBlt( bmpDC, 0, 0, Width, Height, fromDC, x, y, SRCCOPY );*/
}
//---------------------------------------------------------------------------
GTK2Bitmap::GTK2Bitmap( intf_thread_t *p_intf, Bitmap *c )
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

    BitBlt( bmpDC, 0, 0, Width, Height, ( (GTK2Bitmap *)c )->GetBmpDC(),
            0, 0, SRCCOPY );
    DeleteObject( HBuf );*/
}
//---------------------------------------------------------------------------
GTK2Bitmap::~GTK2Bitmap()
{
    if( Bmp )
        g_object_unref( G_OBJECT( Bmp) );
}
//---------------------------------------------------------------------------
void GTK2Bitmap::DrawBitmap( int x, int y, int w, int h, int xRef, int yRef,
                              Graphics *dest )
{
    if( Bmp )
    {
        GdkDrawable *destImg = ( (GTK2Graphics *)dest )->GetImage();
        GdkGC *destGC = ( (GTK2Graphics *)dest )->GetGC();

        gdk_pixbuf_render_to_drawable( Bmp, destImg, destGC, x, y, xRef, yRef, 
                w, h, GDK_RGB_DITHER_NORMAL, 0, 0);
    }
}
//---------------------------------------------------------------------------
bool GTK2Bitmap::Hit( int x, int y)
{
    unsigned int c = (unsigned int)GetBmpPixel( x, y );

    if( c == -1 || c == AlphaColor )
        return false;
    else
        return true;
}
//---------------------------------------------------------------------------
int GTK2Bitmap::GetBmpPixel( int x, int y )
{
    if( !Bmp || x < 0 || x >= Width || y < 0 || y >= Height )
        return -1;

    guchar *pixels;
    int rowstride, offset;
    gboolean has_alpha;

    rowstride = gdk_pixbuf_get_rowstride( Bmp );
    pixels    = gdk_pixbuf_get_pixels( Bmp ); 
    has_alpha = gdk_pixbuf_get_has_alpha( Bmp );

    offset = y * rowstride + ( x * (has_alpha ? 4 : 3) );

    int r = pixels [offset];
    int g = pixels [offset + 1] << 8;
    int b = pixels [offset + 2] << 16;

    return r + g + b;
}
//---------------------------------------------------------------------------
void GTK2Bitmap::SetBmpPixel( int x, int y, int color )
{
//    SetPixelV( bmpDC, x, y, color );
}
//---------------------------------------------------------------------------

#endif
