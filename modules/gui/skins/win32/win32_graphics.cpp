/*****************************************************************************
 * win32_graphics.cpp: Win32 implementation of the Graphics and Region classes
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: win32_graphics.cpp,v 1.7 2003/05/13 19:25:59 asmax Exp $
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
#include <windows.h>

//--- SKIN ------------------------------------------------------------------
#include "../src/graphics.h"
#include "../src/window.h"
#include "../os_window.h"
#include "win32_graphics.h"



//---------------------------------------------------------------------------
// WIN32 GRAPHICS
//---------------------------------------------------------------------------
Win32Graphics::Win32Graphics( intf_thread_t *p_intf, int w, int h, SkinWindow *from ) : Graphics( w, h )
{
    HBITMAP HImage ;
    Image          = CreateCompatibleDC( NULL );
    if( from != NULL )
    {
        HDC DC = GetWindowDC( ( (Win32Window *)from )->GetHandle() );
        HImage = CreateCompatibleBitmap( DC, w, h );
        ReleaseDC( ( (Win32Window *)from )->GetHandle(), DC );
    }
    else
    {
        HImage = CreateCompatibleBitmap( Image, w, h );
    }
    SelectObject( Image, HImage );
    DeleteObject( HImage );
}
//---------------------------------------------------------------------------
Win32Graphics::~Win32Graphics()
{
    DeleteDC( Image );
}
//---------------------------------------------------------------------------
void Win32Graphics::CopyFrom( int dx, int dy, int dw, int dh, Graphics *Src,
                              int sx, int sy, int Flag )
{
    BitBlt( Image, dx, dy, dw, dh, ( (Win32Graphics *)Src )->GetImageHandle(),
        sx, sy, Flag );
}
//---------------------------------------------------------------------------
void Win32Graphics::DrawRect( int x, int y, int w, int h, int color )
{
    LPRECT r = new RECT;
    HBRUSH  Brush = CreateSolidBrush( color );
    r->left   = x;
    r->top    = y;
    r->right  = x + w;
    r->bottom = y + h;
    FillRect( Image, r, Brush );
    DeleteObject( Brush );
    delete r;
}
//---------------------------------------------------------------------------
void Win32Graphics::SetClipRegion( SkinRegion *rgn )
{
    SelectClipRgn( Image, ( (Win32Region *)rgn )->GetHandle() );
}
//---------------------------------------------------------------------------
void Win32Graphics::ResetClipRegion()
{
    HRGN rgn = CreateRectRgn( 0, 0, Width, Height );
    SelectClipRgn( Image, rgn );
    DeleteObject( rgn );
}
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
// WIN32 REGION
//---------------------------------------------------------------------------
Win32Region::Win32Region()
{
    Rgn = CreateRectRgn( 0, 0, 0, 0 );
}
//---------------------------------------------------------------------------
Win32Region::Win32Region( int x, int y, int w, int h )
{
    Rgn = CreateRectRgn( x, y, x + w, y + h );
}
//---------------------------------------------------------------------------
Win32Region::~Win32Region()
{
    DeleteObject( Rgn );
}
//---------------------------------------------------------------------------
void Win32Region::AddPoint( int x, int y )
{
    AddRectangle( x, y, x + 1, y + 1 );
}
//---------------------------------------------------------------------------
void Win32Region::AddRectangle( int x, int y, int w, int h )
{
    HRGN Buffer;
    Buffer = CreateRectRgn( x, y, x + w, y + h );
    CombineRgn( Rgn, Buffer, Rgn, 0x2 );
    DeleteObject( Buffer );
}
//---------------------------------------------------------------------------
void Win32Region::AddElipse( int x, int y, int w, int h )
{
    HRGN Buffer;
    Buffer = CreateEllipticRgn( x, y, x + w, y + h );
    CombineRgn( Rgn, Buffer, Rgn, 0x2 );
    DeleteObject( Buffer );
}
//---------------------------------------------------------------------------
void Win32Region::Move( int x, int y )
{
    OffsetRgn( Rgn, x, y );
}
//---------------------------------------------------------------------------
bool Win32Region::Hit( int x, int y )
{
    return PtInRegion( Rgn, x, y );
}
//---------------------------------------------------------------------------

#endif
