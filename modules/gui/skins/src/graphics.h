/*****************************************************************************
 * graphics.h: Graphics and Region classes
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: graphics.h,v 1.2 2003/04/19 02:34:47 karibu Exp $
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


#ifndef VLC_SKIN_GRAPHICS
#define VLC_SKIN_GRAPHICS

//---------------------------------------------------------------------------
class Region;

//---------------------------------------------------------------------------
class Graphics
{
    protected:
        int Width;
        int Height;

    public:
        // Constructor
        Graphics( int w, int h );
        // Destructor
        virtual ~Graphics();
        // Drawing methods
        virtual void CopyFrom( int dx, int dy, int dw, int dh, Graphics *Src,
                               int sx, int sy, int Flag ) = 0;
        //virtual void CopyTo( Graphics *Dest, int dx, int dy, int dw, int dh,
        //                     int sx, int sy, int Flag ) = 0;
        virtual void DrawRect( int x, int y, int w, int h, int color ) = 0;

        // Clipping methods
        virtual void SetClipRegion( Region *rgn ) = 0;
        virtual void ResetClipRegion() = 0;

};
//---------------------------------------------------------------------------
class Region
{
    private:
    public:
        // Constructor
        Region();
        Region( int x, int y, int w, int h );
        // Destructor
        virtual ~Region();
        // Modify region
        virtual void Move( int x, int y ) = 0;
        virtual void AddPoint( int x, int y ) = 0;
        virtual void AddRectangle( int x, int y, int w, int h ) = 0;
        virtual void AddElipse( int x, int y, int w, int h ) = 0;

        virtual bool Hit( int x, int y ) = 0;
};
//---------------------------------------------------------------------------

#endif
