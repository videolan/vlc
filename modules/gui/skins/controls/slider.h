/*****************************************************************************
 * slider.h: Slider control
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: slider.h,v 1.3 2003/04/20 15:00:19 karibu Exp $
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


#ifndef VLC_SKIN_CONTROL_SLIDER
#define VLC_SKIN_CONTROL_SLIDER

//--- GENERAL ---------------------------------------------------------------
#include <string>
using namespace std;

//---------------------------------------------------------------------------
class Event;
class Graphics;
class Window;
class Bezier;
class Region;

//---------------------------------------------------------------------------
class ControlSlider : public GenericControl
{
    private:
        string Type;
        string cursorUp;
        string cursorDown;
        Bezier *Curve;
        bool Selected;
        Event *UpdateEvent;
        bool Enabled;       // Is the button active

        // Cursor properties
        int CWidth;         // Width of cursor
        int CHeight;        // Height of cursor
        int * CursorX;      // Array of x coordinates of slider points
        int * CursorY;      // Array of y coordinates of slider points
        Region *HitRgn;     // Active region for mouse events
        int LastRefreshTime;

        // Slider properties
        int SliderRange;    // Should stay to SLIDER_RANGE
        int MaxValue;       // Maximum value of the slider
        int Value;          // Value of slider

        // ToolTip text
        string BaseToolTipText;
        string FullToolTipText;

        int FindNearestPoint( int x, int y );

        // Move cursor (whether SLIDER_MAX in skin_common.h)
        void MoveCursor( int newValue );


    public:
        // Constructor
        ControlSlider( string id, bool visible, string type, string cursorUp,
            string cursorDown, double *ptx, double *pty, int nb,
            string tooltiptext, string help, Window *Parent );

        // Destructor
        virtual ~ControlSlider();

        // initialization
        virtual void Init();
        virtual bool ProcessEvent( Event *evt );

        // Draw control
        virtual void Draw( int x, int y, int w, int h, Graphics *dest );

        // Mouse events
        virtual bool MouseUp( int x, int y, int button );
        virtual bool MouseDown( int x, int y, int button );
        virtual bool MouseMove( int x, int y, int button );
        virtual bool MouseOver( int x, int y );
        virtual bool ToolTipTest( int x, int y );
        virtual bool MouseScroll( int x, int y, int direction );

        // Slider calls
        void SetCursorPosition( long Pos );
        long GetCursorPosition();

        // Enabling control
        virtual void Enable( Event *event, bool enabled );

        // Translate control
        virtual void MoveRelative( int xOff, int yOff );

        // Change SliderRange (do not use if not sure)
        void ChangeSliderRange( int NewRange );
};
//---------------------------------------------------------------------------

#endif
