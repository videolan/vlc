/*****************************************************************************
 * gtk2_font.h: GTK2 implementation of the Font class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: gtk2_font.h,v 1.5 2003/04/17 15:43:29 karibu Exp $
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


#ifndef VLC_SKIN_GTK2_FONT
#define VLC_SKIN_GTK2_FONT

//--- GENERAL ---------------------------------------------------------------
#include <string>
using namespace std;

//--- GTK2 -----------------------------------------------------------------
#include <gdk/gdk.h>

//---------------------------------------------------------------------------
struct intf_thread_t;
class Graphics;

//---------------------------------------------------------------------------
class GTK2Font : Font
{
    private:
        PangoContext  *Context;
        PangoLayout   *Layout;
        PangoFontDescription *FontDesc;

        // Assign font to Device Context
        virtual void AssignFont( Graphics *dest );

        // Helper function
        virtual void GenericPrint( Graphics *dest, string text, int x, int y,
                                   int w, int h, int align, int color );

    public:
        // Constructor
        GTK2Font( intf_thread_t *_p_intf, string fontname, int size, int color,
                   int weight, bool italic, bool underline );

        // Destructor
        virtual ~GTK2Font();

        // Get size of text
        virtual void GetSize( string text, int &w, int &h );

        // Draw text with boundaries
        virtual void Print( Graphics *dest, string text, int x, int y, int w,
                            int h, int align );

        virtual void PrintColor( Graphics *dest, string text, int x, int y,
                                 int w, int h, int align, int color );

};
//---------------------------------------------------------------------------

#endif
