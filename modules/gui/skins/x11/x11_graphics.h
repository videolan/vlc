/*****************************************************************************
 * x11_graphics.h: X11 implementation of the Graphics and Region classes
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: x11_graphics.h,v 1.3 2003/05/26 02:09:27 gbazin Exp $
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
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


#ifndef VLC_SKIN_X11_GRAPHICS
#define VLC_SKIN_X11_GRAPHICS

//--- GENERAL ---------------------------------------------------------------
#include <vector>
using namespace std;

//--- X11 -------------------------------------------------------------------
#include <X11/Xlib.h>
#include <X11/Xutil.h>

//---------------------------------------------------------------------------
class SkinRegion;
class SkinWindow;

struct CoordsPoint{ int x, y; };
struct CoordsRectangle{ int x, y, w, h; };
struct CoordsElipse{ int x, y, w, h; };

//---------------------------------------------------------------------------
class X11Graphics : public Graphics
{
    protected:
        Display *display;
        Drawable Image;
        GC Gc;

    public:
        // Constructor
        X11Graphics( intf_thread_t *p_intf, int w, int h, SkinWindow *from = NULL );
        // Destructor
        virtual ~X11Graphics();
        // Drawing methods
        virtual void CopyFrom( int dx, int dy, int dw, int dh, Graphics *Src,
                              int sx, int sy, int Flag );
        //virtual void CopyTo(  Graphics *Dest, int dx, int dy, int dw, int dh,
        //                      int sx, int sy, int Flag );
        virtual void DrawRect( int x, int y, int w, int h, int color );

        // Clipping methods
        virtual void SetClipRegion( SkinRegion *rgn );
        virtual void ResetClipRegion();

        // Specific X11 methods
        Drawable GetImage() { return Image; };
        GC GetGC()    { return Gc; };
};
//---------------------------------------------------------------------------
class X11Region : public SkinRegion
{
    private:
        Region *Rgn;
    public:
        // Constructor
        X11Region();
        X11Region( int x, int y, int w, int h );
        // Destructor
        ~X11Region();
        // Modify region
        virtual void AddPoint( int x, int y );
        virtual void AddRectangle( int x, int y, int w, int h );
        virtual void AddElipse( int x, int y, int w, int h );
        virtual void Move( int x, int y );

        virtual bool Hit( int x, int y );

        // Specific X11 methods
        Region *GetHandle() { return Rgn; };

        vector<CoordsRectangle> RectanglesList;
        vector<CoordsElipse> ElipsesList;

        CoordsPoint RefPoint;
};
//---------------------------------------------------------------------------

#endif
