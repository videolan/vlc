/*****************************************************************************
 * win32_bitmap.h: Win32 implementation of the Bitmap class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: win32_bitmap.h,v 1.2 2003/04/12 21:43:27 asmax Exp $
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

#ifndef VLC_WIN32_BITMAP
#define VLC_WIN32_BITMAP

//--- WIN32 -----------------------------------------------------------------
#include <windows.h>

//--- GENERAL ---------------------------------------------------------------
#include <string>
using namespace std;

//---------------------------------------------------------------------------
struct intf_thread_t;
class Bitmap;
class Graphics;

//---------------------------------------------------------------------------
class Win32Bitmap : public Bitmap
{
    private:
        HDC bmpDC;

    public:
        // Constructors
        Win32Bitmap( intf_thread_t *p_intf, string FileName, int AColor );
        Win32Bitmap( intf_thread_t *p_intf, Graphics *from, int x, int y,
                     int w, int h, int AColor );
        Win32Bitmap( intf_thread_t *p_intf, Bitmap *c );

        // Destructor
        virtual ~Win32Bitmap();

        virtual void DrawBitmap( int x, int y, int w, int h, int xRef, int yRef,
                                 Graphics *dest );
        virtual bool Hit( int x, int y );

        virtual int  GetBmpPixel( int x, int y );
        virtual void SetBmpPixel( int x, int y, int color );

        HDC GetBmpDC() { return bmpDC; }
};
//---------------------------------------------------------------------------

#endif

#endif
