/*****************************************************************************
 * button.h: Button control
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: button.h,v 1.2 2003/03/19 02:09:56 videolan Exp $
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


#ifndef VLC_SKIN_CONTROL_BUTTON
#define VLC_SKIN_CONTROL_BUTTON

//--- GENERAL ---------------------------------------------------------------
#include <string>
using namespace std;

//---------------------------------------------------------------------------
class Action;
class Graphics;
class Window;

//---------------------------------------------------------------------------
class ControlButton : public GenericControl
{
    private:
        // Image IDs
        string      Up;
        string      Down;
        string      Disabled;

        // Control behaviour
        bool        Selected;
        bool        Enabled;
        bool        CursorIn;

        // List of actions to execute
        Action     *ClickAction;
        string      ClickActionName;
        Action     *MouseOverAction;
        string      MouseOverActionName;
        Action     *MouseOutAction;
        string      MouseOutActionName;

        // ToolTip text
        string      ToolTipText;

    public:
        // Constructor
        ControlButton( string id,
                       bool visible,
                       int x, int y,
                       string Up, string Down, string Disabled,
                       string onclick, string onmousevoer, string onmouseout,
                       string tooltiptext, string help,
                       Window *Parent );

        // Destructor
        virtual ~ControlButton();

        // Initializations
        virtual void Init();
        virtual bool ProcessEvent( Event *evt );

        // Draw button
        virtual void Draw( int x1, int y1, int x2, int y2, Graphics *dest );

        // Mouse events
        virtual bool MouseUp( int x, int y, int button );
        virtual bool MouseDown( int x, int y, int button );
        virtual bool MouseMove( int x, int y, int button );
        virtual bool MouseOver( int x, int y );
        virtual bool ToolTipTest( int x, int y );

        // Translate control
        virtual void MoveRelative( int xOff, int yOff );

        // Enabling control
        virtual void Enable( Event *event, bool enabled );
};
//---------------------------------------------------------------------------

#endif
