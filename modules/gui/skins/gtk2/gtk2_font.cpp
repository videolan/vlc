/*****************************************************************************
 * gtk2_font.cpp: GTK2 implementation of the Font class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: gtk2_font.cpp,v 1.9 2003/04/17 17:45:38 asmax Exp $
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

//--- VLC -------------------------------------------------------------------
#include <vlc/intf.h>

//--- SKIN ------------------------------------------------------------------
#include "../src/graphics.h"
#include "../os_graphics.h"
#include "../src/font.h"
#include "../os_font.h"



//---------------------------------------------------------------------------
// Font object
//---------------------------------------------------------------------------
GTK2Font::GTK2Font( intf_thread_t *_p_intf, string fontname, int size,
    int color, int weight, bool italic, bool underline )
    : Font( _p_intf, fontname, size, color, weight, italic, underline )
{
    Context = gdk_pango_context_get();
    Layout = pango_layout_new( Context );

    // Text properties setting
    FontDesc = pango_font_description_new();

    pango_font_description_set_family( FontDesc, fontname.c_str() );

    pango_font_description_set_size( FontDesc, size * PANGO_SCALE );

    if( italic )
        pango_font_description_set_style( FontDesc, PANGO_STYLE_ITALIC );
    else
        pango_font_description_set_style( FontDesc, PANGO_STYLE_NORMAL );

    pango_font_description_set_weight( FontDesc, (PangoWeight)weight );

    /* FIXME: underline parameter */

    // Set attributes
    pango_layout_set_font_description( Layout, FontDesc );
}
//---------------------------------------------------------------------------
GTK2Font::~GTK2Font()
{
}
//---------------------------------------------------------------------------
void GTK2Font::AssignFont( Graphics *dest )
{
}
//---------------------------------------------------------------------------
void GTK2Font::GetSize( string text, int &w, int &h )
{
    pango_layout_set_text( Layout, text.c_str(), text.length() );
    pango_layout_get_pixel_size( Layout, &w, &h );
}
//---------------------------------------------------------------------------
void GTK2Font::GenericPrint( Graphics *dest, string text, int x, int y,
                                 int w, int h, int align, int color )
{
    // Get handles
    GdkDrawable *drawable = ( (GTK2Graphics *)dest )->GetImage();
    GdkGC *gc = ( (GTK2Graphics *)dest )->GetGC();

    // Set text
    pango_layout_set_text( Layout, text.c_str(), text.length() );
    pango_layout_set_width( Layout, w * PANGO_SCALE );

    // Set attributes
    pango_layout_set_alignment( Layout, (PangoAlignment)align );
    gdk_rgb_gc_set_foreground( gc, color );

    // Render text
    gdk_draw_layout( drawable, gc, x, y, Layout );
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
