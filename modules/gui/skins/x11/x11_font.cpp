/*****************************************************************************
 * x11_font.cpp: X11 implementation of the Font class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: x11_font.cpp,v 1.5 2003/06/01 22:11:24 asmax Exp $
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

//--- X11 -----------------------------------------------------------------
#include <X11/Xlib.h>

//--- VLC -------------------------------------------------------------------
#include <vlc/intf.h>

//--- SKIN ------------------------------------------------------------------
#include "../src/skin_common.h"
#include "../src/graphics.h"
#include "../os_graphics.h"
#include "../src/font.h"
#include "../os_font.h"
#include "../src/theme.h"
#include "../os_theme.h"

//---------------------------------------------------------------------------
// Font object
//---------------------------------------------------------------------------
X11Font::X11Font( intf_thread_t *_p_intf, string fontname, int size,
    int color, int weight, bool italic, bool underline )
    : SkinFont( _p_intf, fontname, size, color, weight, italic, underline )
{
    display = g_pIntf->p_sys->display;

    // FIXME: just a beginning...
    XLOCK;
    font = XLoadFont( display, "-misc-fixed-*-*-*-*-*-*-*-*-*-*-*-*" );
    XUNLOCK;
}
//---------------------------------------------------------------------------
X11Font::~X11Font()
{
}
//---------------------------------------------------------------------------
void X11Font::AssignFont( Graphics *dest )
{
}
//---------------------------------------------------------------------------
void X11Font::GetSize( string text, int &w, int &h )
{
    int direction, fontAscent, fontDescent;
    XCharStruct overall;
    
    XLOCK;
    XQueryTextExtents( display, font, text.c_str(), text.size(), 
                        &direction, &fontAscent, &fontDescent, &overall );
    XUNLOCK;

    w = overall.rbearing - overall.lbearing;
    h = overall.ascent + overall.descent;
}
//---------------------------------------------------------------------------
void X11Font::GenericPrint( Graphics *dest, string text, int x, int y,
                                 int w, int h, int align, int color )
{
    // Get handles
    Drawable drawable = ( (X11Graphics *)dest )->GetImage();
    GC gc = ( (X11Graphics *)dest )->GetGC();

    XGCValues gcVal;
    gcVal.foreground = (color == 0 ? 10 : color);
    gcVal.font = font;
    
    // Render text on buffer
    XLOCK;
    XChangeGC( display, gc, GCForeground|GCFont,  &gcVal );
    XDrawString( display, drawable, gc, x, y+h, text.c_str(), 
                 text.size());
    XUNLOCK;
}

//---------------------------------------------------------------------------
void X11Font::Print( Graphics *dest, string text, int x, int y, int w,
                       int h, int align )
{
    GenericPrint( dest, text, x, y, w, h, align, Color );
}
//---------------------------------------------------------------------------
void X11Font::PrintColor( Graphics *dest, string text, int x, int y, int w,
                            int h, int align, int color )
{
    GenericPrint( dest, text, x, y, w, h, align, color );
}
//---------------------------------------------------------------------------

#endif
