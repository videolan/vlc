/*****************************************************************************
 * gtk2_graphics.h: GTK2 implementation of the Graphics and Region classes
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: gtk2_graphics.h,v 1.4 2003/04/19 02:34:47 karibu Exp $
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


#ifndef VLC_SKIN_GTK2_GRAPHICS
#define VLC_SKIN_GTK2_GRAPHICS

//--- GTK2 ------------------------------------------------------------------
#include <gdk/gdk.h>

//---------------------------------------------------------------------------
class Region;
class Window;

//---------------------------------------------------------------------------
class GTK2Graphics : public Graphics
{
    protected:
        GdkDrawable *Image;
        GdkGC *Gc;

    public:
        // Constructor
        GTK2Graphics( int w, int h, Window *from = NULL );
        // Destructor
        virtual ~GTK2Graphics();
        // Drawing methods
        virtual void CopyFrom( int dx, int dy, int dw, int dh, Graphics *Src,
                              int sx, int sy, int Flag );
        //virtual void CopyTo(  Graphics *Dest, int dx, int dy, int dw, int dh,
        //                      int sx, int sy, int Flag );
        virtual void DrawRect( int x, int y, int w, int h, int color );

        // Clipping methods
        virtual void SetClipRegion( Region *rgn );
        virtual void ResetClipRegion();

        // Specific GTK2 methods
        GdkDrawable *GetImage() { return Image; };
        GdkGC *GetGC()    { return Gc; };
};
//---------------------------------------------------------------------------
class GTK2Region : public Region
{
    private:
        GdkRegion *Rgn;
    public:
        // Constructor
        GTK2Region();
        GTK2Region( int x, int y, int w, int h );
        // Destructor
        ~GTK2Region();
        // Modify region
        virtual void AddPoint( int x, int y );
        virtual void AddRectangle( int x, int y, int w, int h );
        virtual void AddElipse( int x, int y, int w, int h );
        virtual void Move( int x, int y );

        virtual bool Hit( int x, int y );

        // Specific GTK2 methods
        GdkRegion *GetHandle() { return Rgn; };
};
//---------------------------------------------------------------------------

#endif
