/*****************************************************************************
 * gtk2_graphics.cpp: GTK2 implementation of the Graphics and Region classes
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: gtk2_graphics.cpp,v 1.13 2003/04/19 02:34:47 karibu Exp $
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

#if !defined WIN32

//--- GTK2 -----------------------------------------------------------------
#include <gdk/gdk.h>

//--- SKIN ------------------------------------------------------------------
#include "../src/graphics.h"
#include "../src/window.h"
#include "../os_window.h"
#include "gtk2_graphics.h"

#include <stdio.h>
#include <math.h>

//---------------------------------------------------------------------------
// GTK2 GRAPHICS
//---------------------------------------------------------------------------
GTK2Graphics::GTK2Graphics( int w, int h, Window *from ) : Graphics( w, h )
{
    if( from != NULL )
    {
        GdkWindow *fromWnd = ( (GTK2Window *)from )->GetHandle();
        Image = (GdkDrawable*) gdk_pixmap_new( fromWnd, w, h, -1 );
        Gc = gdk_gc_new( ( GdkDrawable* )fromWnd );
    }
    else
    {
        Image = (GdkDrawable*) gdk_pixmap_new( NULL, w, h, 8 );
        gdk_drawable_set_colormap( Image, gdk_colormap_get_system() );
        Gc = gdk_gc_new( Image );
    }

    // Set the background color to black
    gdk_draw_rectangle( Image, Gc, TRUE, 0, 0, w, h );
}
//---------------------------------------------------------------------------
GTK2Graphics::~GTK2Graphics()
{
    g_object_unref( Gc );
    g_object_unref( Image );
}
//---------------------------------------------------------------------------
void GTK2Graphics::CopyFrom( int dx, int dy, int dw, int dh, Graphics *Src,
                              int sx, int sy, int Flag )
{
    gdk_draw_drawable( Image, Gc, (( GTK2Graphics* )Src )->GetImage(),
            sx, sy, dx, dy, dw, dh );
}
//---------------------------------------------------------------------------
void GTK2Graphics::DrawRect( int x, int y, int w, int h, int color )
{
    gdk_draw_rectangle( Image, Gc, TRUE, x, y, w, h);
}
//---------------------------------------------------------------------------
void GTK2Graphics::SetClipRegion( Region *rgn )
{
    gdk_gc_set_clip_region( Gc, ( (GTK2Region *)rgn )->GetHandle() );
}
//---------------------------------------------------------------------------
void GTK2Graphics::ResetClipRegion()
{
    GdkRectangle rect;
    rect.x = 0;
    rect.y = 0;
    rect.width = Width;
    rect.height = Height;
    GdkRegion *rgn = gdk_region_rectangle( &rect );
    gdk_gc_set_clip_region( Gc, rgn );
    gdk_region_destroy( rgn );
}
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
// GTK2 REGION
//---------------------------------------------------------------------------
GTK2Region::GTK2Region()
{
    Rgn = gdk_region_new();
}
//---------------------------------------------------------------------------
GTK2Region::GTK2Region( int x, int y, int w, int h )
{
    GdkRectangle rect;
    rect.x = x;
    rect.y = y;
    rect.width = w;
    rect.height = h;
    Rgn = gdk_region_rectangle( &rect );
}
//---------------------------------------------------------------------------
GTK2Region::~GTK2Region()
{
    gdk_region_destroy( Rgn );
}
//---------------------------------------------------------------------------
void GTK2Region::AddPoint( int x, int y )
{
    AddRectangle( x, y, 1, 1 );
}
//---------------------------------------------------------------------------
void GTK2Region::AddRectangle( int x, int y, int w, int h )
{
    GdkRectangle rect;
    rect.x = x;
    rect.y = y;
    rect.width = w;
    rect.height = h;
    GdkRegion *Buffer = gdk_region_rectangle( &rect );
    gdk_region_union( Rgn, Buffer );
}
//---------------------------------------------------------------------------
void GTK2Region::AddElipse( int x, int y, int w, int h )
{
    GdkRegion *Buffer;
    GdkRectangle rect;
    rect.height = 1;

    double ex, ey;
    double a = w / 2;
    double b = h / 2;

    if( !a || !b )
        return;

    for( ey = 0; ey < h; ey++ )
    {
        // Calculate coords
        ex = a * sqrt( 1 - ey * ey / ( b * b ) );

        // Upper line
        rect.x     = (gint)( x + a - ex );
        rect.y     = (gint)( y + b - ey );
        rect.width = (gint)( 2 * ex );
        Buffer = gdk_region_rectangle( &rect );
        gdk_region_union( Rgn, Buffer );
        gdk_region_destroy( Buffer );

        // Lower line
        rect.y = (gint)( y + b + ey );
        Buffer = gdk_region_rectangle( &rect );
        gdk_region_union( Rgn, Buffer );
        gdk_region_destroy( Buffer );
    }
}
//---------------------------------------------------------------------------
void GTK2Region::Move( int x, int y )
{
    gdk_region_offset( Rgn, x, y );
}
//---------------------------------------------------------------------------
bool GTK2Region::Hit( int x, int y )
{
    return gdk_region_point_in( Rgn, x, y );
}
//---------------------------------------------------------------------------

#endif
