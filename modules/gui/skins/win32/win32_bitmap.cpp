/*****************************************************************************
 * win32_bitmap.cpp: Win32 implementation of the Bitmap class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: win32_bitmap.cpp,v 1.6 2003/04/29 12:54:57 gbazin Exp $
 *
 * Authors: Olivier Teulière <ipkiss@via.ecp.fr>
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

#ifdef WIN32

//--- WIN32 -----------------------------------------------------------------
#define WINVER  0x0500
#include <windows.h>

//--- VLC -------------------------------------------------------------------
#include <vlc/intf.h>

//--- SKIN ------------------------------------------------------------------
#include "../os_api.h"
#include "../src/graphics.h"
#include "win32_graphics.h"
#include "../src/bitmap.h"
#include "win32_bitmap.h"
#include "../src/skin_common.h"



//---------------------------------------------------------------------------
//   Win32Bitmap
//---------------------------------------------------------------------------
Win32Bitmap::Win32Bitmap( intf_thread_t *p_intf, string FileName, int AColor )
    : Bitmap( p_intf, FileName, AColor )
{
    HBITMAP HBitmap;
    HBITMAP HBuf;
    BITMAP  Bmp;
    HDC     bufDC;

    // Create image from file if it exists
    HBitmap = (HBITMAP) LoadImage( NULL, FileName.c_str(), IMAGE_BITMAP,
                                   0, 0, LR_LOADFROMFILE );
    if( HBitmap == NULL )
    {
        if( FileName != "" )
            msg_Warn( p_intf, "Couldn't load bitmap: %s", FileName.c_str() );

        HBitmap = CreateBitmap( 0, 0, 1, 32, NULL );
    }

    // Create device context
    bmpDC   = CreateCompatibleDC( NULL );
    SelectObject( bmpDC, HBitmap );

    // Get size of image
    GetObject( HBitmap, sizeof( Bmp ), &Bmp );
    Width  = Bmp.bmWidth;
    Height = Bmp.bmHeight;

    // If alpha color is not 0, then change 0 color to non black color to avoid
    // window transparency
    if( (int)AlphaColor != OSAPI_GetNonTransparentColor( 0 ) )
    {
        bufDC = CreateCompatibleDC( bmpDC );
        HBuf = CreateCompatibleBitmap( bmpDC, Width, Height );
        SelectObject( bufDC, HBuf );

        LPRECT r = new RECT;
        HBRUSH Brush = CreateSolidBrush( OSAPI_GetNonTransparentColor( 0 ) );
        r->left   = 0;
        r->top    = 0;
        r->right  = Width;
        r->bottom = Height;
        FillRect( bufDC, r, Brush );
        DeleteObject( Brush );
        delete r;

        if( p_intf->p_sys->TransparentBlt && IS_WINNT )
        {
            // This function contains a memory leak on win95/win98
            p_intf->p_sys->TransparentBlt( bufDC, 0, 0, Width, Height,
                                           bmpDC, 0, 0, Width, Height, 0 );
        }
        else
        {
            BitBlt( bufDC, 0, 0, Width, Height, bmpDC, 0, 0, SRCCOPY );
        }

        BitBlt( bmpDC, 0, 0, Width, Height, bufDC, 0, 0, SRCCOPY );
        DeleteDC( bufDC );
        DeleteObject( HBuf );
    }

    // Delete objects
    DeleteObject( HBitmap );
}
//---------------------------------------------------------------------------
Win32Bitmap::Win32Bitmap( intf_thread_t *p_intf, Graphics *from, int x, int y,
    int w, int h, int AColor ) : Bitmap( p_intf, from, x, y, w, h, AColor )
{
    Width  = w;
    Height = h;
    AlphaColor = AColor;
    HBITMAP HBmp;
    HDC fromDC = ( (Win32Graphics *)from )->GetImageHandle();

    // Create image
    bmpDC = CreateCompatibleDC( fromDC );
    HBmp  = CreateCompatibleBitmap( fromDC, Width, Height );
    SelectObject( bmpDC, HBmp );
    DeleteObject( HBmp );
    BitBlt( bmpDC, 0, 0, Width, Height, fromDC, x, y, SRCCOPY );
}
//---------------------------------------------------------------------------
Win32Bitmap::Win32Bitmap( intf_thread_t *p_intf, Bitmap *c )
    : Bitmap( p_intf, c )
{
    HBITMAP HBuf;

    // Copy attibutes
    c->GetSize( Width, Height );
    AlphaColor = c->GetAlphaColor();

    // Copy bmpDC
    bmpDC = CreateCompatibleDC( NULL );
    HBuf  = CreateCompatibleBitmap( bmpDC, Width, Height );
    SelectObject( bmpDC, HBuf );

    BitBlt( bmpDC, 0, 0, Width, Height, ( (Win32Bitmap *)c )->GetBmpDC(),
            0, 0, SRCCOPY );
    DeleteObject( HBuf );
}
//---------------------------------------------------------------------------
Win32Bitmap::~Win32Bitmap()
{
    DeleteDC( bmpDC );
}
//---------------------------------------------------------------------------
void Win32Bitmap::DrawBitmap( int x, int y, int w, int h, int xRef, int yRef,
                              Graphics *dest )
{
    HDC destDC = ( (Win32Graphics *)dest )->GetImageHandle();

    if( p_intf->p_sys->TransparentBlt && IS_WINNT )
    {
        // This function contains a memory leak on win95/win98
        p_intf->p_sys->TransparentBlt( destDC, xRef, yRef, w, h,
                                       bmpDC, x, y, w, h, AlphaColor );
    }
    else
    {
        BitBlt( destDC, xRef, yRef, w, h, bmpDC, x, y, SRCCOPY );
    }
}
//---------------------------------------------------------------------------
bool Win32Bitmap::Hit( int x, int y)
{
    unsigned int c = GetPixel( bmpDC, x, y );
    if( c == AlphaColor || c == CLR_INVALID )
        return false;
    else
        return true;

}
//---------------------------------------------------------------------------
int Win32Bitmap::GetBmpPixel( int x, int y )
{
    return GetPixel( bmpDC, x, y );
}
//---------------------------------------------------------------------------
void Win32Bitmap::SetBmpPixel( int x, int y, int color )
{
    SetPixelV( bmpDC, x, y, color );
}
//---------------------------------------------------------------------------

#endif
