/*****************************************************************************
 * gtk2_graphics.cpp: GTK2 implementation of the Graphics and Region classes
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: gtk2_graphics.cpp,v 1.3 2003/04/13 17:46:22 asmax Exp $
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


//--- GTK2 -----------------------------------------------------------------
#include <gdk/gdk.h>

//--- SKIN ------------------------------------------------------------------
#include "graphics.h"
#include "window.h"
#include "os_window.h"
#include "gtk2_graphics.h"


//---------------------------------------------------------------------------
// GTK2 GRAPHICS
//---------------------------------------------------------------------------
GTK2Graphics::GTK2Graphics( int w, int h, Window *from ) : Graphics( w, h )
{
/*    HBITMAP HImage ;
    Image          = CreateCompatibleDC( NULL );
    if( from != NULL )
    {
        HDC DC = GetWindowDC( ( (GTK2Window *)from )->GetHandle() );
        HImage = CreateCompatibleBitmap( DC, w, h );
        ReleaseDC( ( (GTK2Window *)from )->GetHandle(), DC );
    }
    else
    {
        HImage = CreateCompatibleBitmap( Image, w, h );
    }
    SelectObject( Image, HImage );
    DeleteObject( HImage );*/
    
/*    Image = ( GdkDrawable* )( (GTK2Window *)from )->GetHandle();
    Gc = gdk_gc_new( Image );*/
}
//---------------------------------------------------------------------------
GTK2Graphics::~GTK2Graphics()
{
/*    DeleteDC( Image );*/
}
//---------------------------------------------------------------------------
void GTK2Graphics::CopyFrom( int dx, int dy, int dw, int dh, Graphics *Src,
                              int sx, int sy, int Flag )
{
/*    BitBlt( Image, dx, dy, dw, dh, ( (GTK2Graphics *)Src )->GetImageHandle(),
        sx, sy, Flag );*/
}

//---------------------------------------------------------------------------
/*void GTK2Graphics::CopyTo( Graphics *Dest, int dx, int dy, int dw, int dh,
                            int sx, int sy, int Flag )
{
    BitBlt( ( (GTK2Graphics *)Dest )->GetImageHandle(), dx, dy, dw, dh, Image,
        sx, sy, Flag );
}*/
//---------------------------------------------------------------------------
void GTK2Graphics::DrawRect( int x, int y, int w, int h, int color )
{
//    gdk_draw_rectangle( Image, Gc, TRUE, x, y, w, h);
}
//---------------------------------------------------------------------------
void GTK2Graphics::SetClipRegion( Region *rgn )
{
/*    SelectClipRgn( Image, ( (GTK2Region *)rgn )->GetHandle() );*/
}
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
// GTK2 REGION
//---------------------------------------------------------------------------
GTK2Region::GTK2Region()
{
/*    Rgn = CreateRectRgn( 0, 0, 0, 0 );*/
}
//---------------------------------------------------------------------------
GTK2Region::GTK2Region( int x, int y, int w, int h )
{
/*    Rgn = CreateRectRgn( x, y, x + w, y + h );*/
}
//---------------------------------------------------------------------------
GTK2Region::~GTK2Region()
{
/*    DeleteObject( Rgn );*/
}
//---------------------------------------------------------------------------
void GTK2Region::AddPoint( int x, int y )
{
/*    AddRectangle( x, y, x + 1, y + 1 );*/
}
//---------------------------------------------------------------------------
void GTK2Region::AddRectangle( int x, int y, int w, int h )
{
/*    HRGN Buffer;
    Buffer = CreateRectRgn( x, y, x + w, y + h );
    CombineRgn( Rgn, Buffer, Rgn, 0x2 );
    DeleteObject( Buffer );*/
}
//---------------------------------------------------------------------------
void GTK2Region::AddElipse( int x, int y, int w, int h )
{
/*    HRGN Buffer;
    Buffer = CreateEllipticRgn( x, y, x + w, y + h );
    CombineRgn( Rgn, Buffer, Rgn, 0x2 );
    DeleteObject( Buffer );*/
}
//---------------------------------------------------------------------------
void GTK2Region::Move( int x, int y )
{
/*    OffsetRgn( Rgn, x, y );*/
}
//---------------------------------------------------------------------------
bool GTK2Region::Hit( int x, int y )
{
/*    return PtInRegion( Rgn, x, y );*/
}
//---------------------------------------------------------------------------

