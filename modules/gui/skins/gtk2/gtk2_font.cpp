/*****************************************************************************
 * gtk2_font.cpp: GTK2 implementation of the Font class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: gtk2_font.cpp,v 1.5 2003/04/16 21:40:07 ipkiss Exp $
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

#if !defined WIN32

//--- GTK2 -----------------------------------------------------------------
#include <gdk/gdk.h>

//--- VLC -------------------------------------------------------------------
#include <vlc/intf.h>

//--- SKIN ------------------------------------------------------------------
#include "../src/graphics.h"
#include "gtk2_graphics.h"
#include "../src/font.h"
#include "gtk2_font.h"



//---------------------------------------------------------------------------
// Font object
//---------------------------------------------------------------------------
GTK2Font::GTK2Font( intf_thread_t *_p_intf, string fontname, int size,
    int color, int weight, bool italic, bool underline )
    : Font( _p_intf, fontname, size, color, weight, italic, underline )
{
/* FIXME */
/*    GFont = gdk_font_load( "-adobe-helvetica-bold-r-normal--12-120-75-75-p-70-iso8859-1" );
    if( GFont == NULL )
    {
        msg_Err( _p_intf, "Could not load font %s", fontname.c_str());
    }*/
}
//---------------------------------------------------------------------------
GTK2Font::~GTK2Font()
{
}
//---------------------------------------------------------------------------
/*void GTK2Font::AssignGTK2Font( GdkDrawable *DC )
{
    // Create font
    HGDIOBJ fontObj = CreateFont(
        -MulDiv( Size, GetDeviceCaps( DC, LOGPIXELSX ), 72 ),
        0,
        0,                  // angle of escapement
        0,                  // base-line orientation angle
        Weight,             // font weight
        Italic,             // italic attribute flag
        Underline,          // underline attribute flag
        0,                  // strikeout attribute flag
        ANSI_CHARSET,       // character set identifier
        OUT_TT_PRECIS,      // output precision
        0,                  // clipping precision
        ANTIALIASED_QUALITY,      // output quality
        0,                  // pitch and family
        FontName.c_str()    // pointer to typeface name string
    );

    // Assign font to DC
    SelectObject( DC, fontObj );

    // Free memory
    DeleteObject( fontObj );
//    GdkGC *gc = gdk_gc_new( DC );
//    gdk_gc_set_font( GFont, gc );
}*/
//---------------------------------------------------------------------------
void GTK2Font::AssignFont( Graphics *dest )
{
/*    GdkDrawable *DC = ( (GTK2Graphics *)dest )->GetImageHandle();
    AssignGTK2Font( DC );*/
}
//---------------------------------------------------------------------------
void GTK2Font::GetSize( string text, int &w, int &h )
{
/*    // Get device context of screen
    HDC DeskDC = GetWindowDC( GetDesktopWindow() );

    // Get size
    LPRECT rect = new RECT;;
    rect->left   = 0;
    rect->top    = 0;
    AssignGTK2Font( DeskDC );
    DrawText( DeskDC, text.c_str(), text.length(), rect, DT_CALCRECT);
    w = rect->right - rect->left;
    h = rect->bottom - rect->top;

    // Release screen device context
    ReleaseDC( GetDesktopWindow(), DeskDC );*/
    /*FIXME*/
/*    w = gdk_text_width( GFont, text.c_str(), text.length() );
    h = gdk_text_height( GFont, text.c_str(), text.length() );*/
    w = 0;
    h = 0;
}
//---------------------------------------------------------------------------
void GTK2Font::GenericPrint( Graphics *dest, string text, int x, int y,
                                 int w, int h, int align, int color )
{
/*    HDC DC = ( (GTK2Graphics *)dest )->GetImageHandle();
    // Set boundaries
    LPRECT r = new RECT;
    r->left   = x;
    r->top    = y;
    r->right  = x + w;
    r->bottom = y + h;

    // Get desktop Device Context
    SetBkMode( DC, TRANSPARENT );

    // Modify desktop attributes
    AssignFont( dest );

    // Change text color
    SetTextColor( DC, color );

    // Draw text on screen
    DrawText( DC, text.c_str(), text.length(), r, align );

    // Set text color to black to avoid graphic bugs
    SetTextColor( DC, 0 );

    // Free memory
    delete r;*/
}

//---------------------------------------------------------------------------
void GTK2Font::Print( Graphics *dest, string text, int x, int y, int w,
                       int h, int align )
{
    GenericPrint( dest, text, x, y, w, h, align, Color );
}
//---------------------------------------------------------------------------
void GTK2Font::PrintColor( Graphics *dest, string text, int x, int y, int w,
                            int h, int align, int color )
{
    GenericPrint( dest, text, x, y, w, h, align, color );
}
//---------------------------------------------------------------------------

#endif
