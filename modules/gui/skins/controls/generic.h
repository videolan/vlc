/*****************************************************************************
 * generic.h: Generic control, parent of the others
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: generic.h,v 1.1 2003/03/18 02:21:47 ipkiss Exp $
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


#ifndef VLC_SKIN_CONTROL_GENERIC
#define VLC_SKIN_CONTROL_GENERIC

//--- GENERAL ---------------------------------------------------------------
#include <string>
using namespace std;

//---------------------------------------------------------------------------
struct intf_thread_t;
class Window;
class Bitmap;
class Graphics;
class Region;
class Event;

//---------------------------------------------------------------------------
// Generic control class
//---------------------------------------------------------------------------
class GenericControl               // This is the generic control class
{
    protected:
        Window * ParentWindow;
        bool     Visible;
        string   ID;
        string   Help;
        intf_thread_t *p_intf;

    private:
    public:
        // Constructor
        GenericControl( string id, bool visible, string help, Window *Parent );

        // Destructor
        virtual ~GenericControl();

        // initializations
        virtual void Init();
        bool GenericProcessEvent( Event *evt );
        virtual bool ProcessEvent( Event *evt );

        // Draw the control into the destination DC
        virtual void Draw( int x, int y, int w, int h, Graphics *dest ) = 0;

        // Simulate a mouse action on control at coordinates x/y
        virtual bool MouseUp( int x, int y, int button );
        virtual bool MouseDown( int x, int y, int button );
        virtual bool MouseMove( int x, int y, int button );
        virtual bool MouseOver( int x, int y );
        virtual bool MouseDblClick( int x, int y, int button );
        virtual bool ToolTipTest( int x, int y );
        virtual bool SendNewHelpText();

        // Move control
        void Move( int left, int top );
        virtual void MoveRelative( int xOff, int yOff );

        // Get two rectangle regions and return intersection
        bool GetIntersectRgn( int x1, int y1, int w1, int h1, int x2,
            int y2, int w2, int h2, int &x, int &y, int &w, int &h );

        // Create a region from a bitmap with transcolor as empty region
        Region *CreateRegionFromBmp( Bitmap *bmp, int MoveX, int MoveY );
        int Left;               // Left offset of the control
        int Top;                // Top offset of the control
        int Width;              // Width of the control
        int Height;             // Height of the control
        int State;              // Used to special state of the control
                                // (for button, sets whether down or up)
        Bitmap **Img;           // Array of bitmap used to draw control

        // Enabling control
        virtual void Enable( Event *event, bool enabled );

        // Found if ID matches
        bool IsID( string id );
};
//---------------------------------------------------------------------------


#endif
