/*****************************************************************************
 * win32_font.cpp: Win32 implementation of the Font class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: win32_font.cpp,v 1.1 2003/03/18 02:21:47 ipkiss Exp $
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


//--- WIN32 -----------------------------------------------------------------
#include <windows.h>

//--- VLC -------------------------------------------------------------------
#include <vlc/intf.h>

//--- SKIN ------------------------------------------------------------------
#include "graphics.h"
#include "win32_graphics.h"
#include "font.h"
#include "win32_font.h"



//---------------------------------------------------------------------------
// Font object
//---------------------------------------------------------------------------
Win32Font::Win32Font( intf_thread_t *_p_intf, string fontname, int size,
    int color, int weight, bool italic, bool underline )
    : Font( _p_intf, fontname, size, color, weight, italic, underline )
{
}
//---------------------------------------------------------------------------
Win32Font::~Win32Font()
{
}
//---------------------------------------------------------------------------
void Win32Font::AssignWin32Font( HDC DC )
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
}
//---------------------------------------------------------------------------
void Win32Font::AssignFont( Graphics *dest )
{
    HDC DC = ( (Win32Graphics *)dest )->GetImageHandle();
    AssignWin32Font( DC );
}
//---------------------------------------------------------------------------
void Win32Font::GetSize( string text, int &w, int &h )
{
    // Get device context of screen
    HDC DeskDC = GetWindowDC( GetDesktopWindow() );

    // Get size
    LPRECT rect = new RECT;;
    rect->left   = 0;
    rect->top    = 0;
    AssignWin32Font( DeskDC );
    DrawText( DeskDC, text.c_str(), text.length(), rect, DT_CALCRECT);
    w = rect->right - rect->left;
    h = rect->bottom - rect->top;

    // Release screen device context
    ReleaseDC( GetDesktopWindow(), DeskDC );
}
//---------------------------------------------------------------------------
void Win32Font::GenericPrint( Graphics *dest, string text, int x, int y,
                                 int w, int h, int align, int color )
{
    HDC DC = ( (Win32Graphics *)dest )->GetImageHandle();
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
    delete r;
}

//---------------------------------------------------------------------------
void Win32Font::Print( Graphics *dest, string text, int x, int y, int w,
                       int h, int align )
{
    GenericPrint( dest, text, x, y, w, h, align, Color );
}
//---------------------------------------------------------------------------
void Win32Font::PrintColor( Graphics *dest, string text, int x, int y, int w,
                            int h, int align, int color )
{
    GenericPrint( dest, text, x, y, w, h, align, color );
}
//---------------------------------------------------------------------------


