/*****************************************************************************
 * win32_graphics.h: Win32 implementation of the Graphics and Region classes
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: win32_graphics.h,v 1.1 2003/03/18 02:21:47 ipkiss Exp $
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


#ifndef VLC_SKIN_WIN32_GRAPHICS
#define VLC_SKIN_WIN32_GRAPHICS

//---------------------------------------------------------------------------
class Region;
class Window;

//---------------------------------------------------------------------------
class Win32Graphics : public Graphics
{
    private:
        int Width;
        int Height;
        HDC Image;
    public:
        // Constructor
        Win32Graphics( int w, int h, Window *from = NULL );
        // Destructor
        virtual ~Win32Graphics();
        // Drawing methods
        virtual void CopyFrom( int dx, int dy, int dw, int dh, Graphics *Src,
                              int sx, int sy, int Flag );
        //virtual void CopyTo(  Graphics *Dest, int dx, int dy, int dw, int dh,
        //                      int sx, int sy, int Flag );
        virtual void DrawRect( int x, int y, int w, int h, int color );

        // Clipping methods
        virtual void SetClipRegion( Region *rgn );

        // Specific win32 methods
        HDC GetImageHandle()    { return Image; };
};
//---------------------------------------------------------------------------
class Win32Region : public Region
{
    private:
        HRGN Rgn;
    public:
        // Constructor
        Win32Region();
        Win32Region( int x, int y, int w, int h );
        // Destructor
        ~Win32Region();
        // Modify region
        virtual void AddPoint( int x, int y );
        virtual void AddRectangle( int x, int y, int w, int h );
        virtual void AddElipse( int x, int y, int w, int h );
        virtual void Move( int x, int y );

        virtual bool Hit( int x, int y );

        // Specific win32 methods
        HRGN GetHandle() { return Rgn; };
};
//---------------------------------------------------------------------------

#endif
