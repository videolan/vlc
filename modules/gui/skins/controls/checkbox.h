/*****************************************************************************
 * checkbox.h: Checkbox control
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: checkbox.h,v 1.3 2003/04/21 21:51:16 asmax Exp $
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


#ifndef VLC_SKIN_CONTROL_CHECKBOX
#define VLC_SKIN_CONTROL_CHECKBOX

//--- GENERAL ---------------------------------------------------------------
#include <string>
using namespace std;

//---------------------------------------------------------------------------
class Action;
class Graphics;
class SkinWindow;

//---------------------------------------------------------------------------
class ControlCheckBox : public GenericControl
{
    private:
        // Image IDs
        string Img1;
        string Img2;
        string Click1;
        string Click2;
        string Disabled1;
        string Disabled2;

        // Behaviour
        bool Enabled1;
        bool Enabled2;
        bool Selected;
        bool CursorIn;

        // List of actions to execute when clicking
        int      Act;
        Action  *ClickAction1;
        string   ClickActionName1;
        Action  *ClickAction2;
        string   ClickActionName2;

        Action     *MouseOverAction1;
        string      MouseOverActionName1;
        Action     *MouseOutAction1;
        string      MouseOutActionName1;
        Action     *MouseOverAction2;
        string      MouseOverActionName2;
        Action     *MouseOutAction2;
        string      MouseOutActionName2;

        // ToolTip text
        string      ToolTipText1;
        string      ToolTipText2;

    public:
        // Constructor
        ControlCheckBox(
            string id,
            bool visible,
            int x, int y,
            string img1, string img2,  string clickimg1, string clickimg2,
            string disabled1, string disabled2,
            string onclick1, string onclick2, string onmouseover1,
            string onmouseout1, string onmouseover2, string onmouseout2,
            string tooltiptext1, string tooltiptext2, string help,
            SkinWindow *Parent );

        // Destructor
        virtual ~ControlCheckBox();

        // initialization
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
