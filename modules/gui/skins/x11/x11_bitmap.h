/*****************************************************************************
 * x11_bitmap.h: X11 implementation of the Bitmap class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: x11_bitmap.h,v 1.1 2003/04/28 14:32:57 asmax Exp $
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


#ifndef VLC_X11_BITMAP
#define VLC_X11_BITMAP

//--- X11 -------------------------------------------------------------------
#include <X11/Xlib.h>

//--- GENERAL ---------------------------------------------------------------
#include <string>
using namespace std;

//---------------------------------------------------------------------------
struct intf_thread_t;
class Bitmap;
class Graphics;

//---------------------------------------------------------------------------
class X11Bitmap : public Bitmap
{
    private:
        Display *display;
        Pixmap Bmp;

    public:
        // Constructors
        X11Bitmap( intf_thread_t *p_intf, string FileName, int AColor );
        X11Bitmap( intf_thread_t *p_intf, Graphics *from, int x, int y,
                     int w, int h, int AColor );
        X11Bitmap( intf_thread_t *p_intf, Bitmap *c );

        // Destructor
        virtual ~X11Bitmap();

        virtual void DrawBitmap( int x, int y, int w, int h, int xRef, int yRef,
                                 Graphics *dest );
        virtual bool Hit( int x, int y );

        virtual int  GetBmpPixel( int x, int y );
        virtual void SetBmpPixel( int x, int y, int color );
};
//---------------------------------------------------------------------------

#endif
