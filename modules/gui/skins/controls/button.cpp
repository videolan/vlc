/*****************************************************************************
 * button.cpp: Button control
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: button.cpp,v 1.10 2003/04/17 13:08:02 karibu Exp $
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


//--- VLC -------------------------------------------------------------------
#include <vlc/intf.h>

//--- SKIN ------------------------------------------------------------------
#include "../src/bitmap.h"
#include "../src/banks.h"
#include "generic.h"
#include "button.h"
#include "../src/event.h"
#include "../src/theme.h"
#include "../src/window.h"
#include "../src/skin_common.h"

//---------------------------------------------------------------------------
// Control Button
//---------------------------------------------------------------------------
ControlButton::ControlButton(
    string id,
    bool visible,
    int x, int y,
    string Up, string Down, string Disabled,
    string onclick, string onmouseover, string onmouseout,
    string tooltiptext, string help,
    Window *Parent ) : GenericControl( id, visible, help, Parent )
{
    // General
    Left            = x;
    Top             = y;
    State           = 1;                   // 1 = up - 0 = down
    Selected        = false;
    Enabled         = true;
    CursorIn        = false;
    this->Up        = Up;
    this->Down      = Down;
    this->Disabled  = Disabled;

    // Actions
    ClickActionName     = onclick;
    MouseOverActionName = onmouseover;
    MouseOutActionName  = onmouseout;

    // Texts
    ToolTipText = tooltiptext;
}
//---------------------------------------------------------------------------
ControlButton::~ControlButton()
{
}
//---------------------------------------------------------------------------
void ControlButton::Init()
{
    // Init bitmaps
    Img = new (Bitmap*)[3];
    Img[0] = p_intf->p_sys->p_theme->BmpBank->Get( Up );
    Img[1] = p_intf->p_sys->p_theme->BmpBank->Get( Down );
    if( Disabled == "none" )
        Img[2] = p_intf->p_sys->p_theme->BmpBank->Get( Up );
    else
        Img[2] = p_intf->p_sys->p_theme->BmpBank->Get( Disabled );

    // Get size of control
    Img[0]->GetSize( Width, Height );

    // Create script
    ClickAction     = new Action( p_intf, ClickActionName );
    MouseOverAction = new Action( p_intf, MouseOverActionName );
    MouseOutAction  = new Action( p_intf, MouseOutActionName );
}
//---------------------------------------------------------------------------
bool ControlButton::ProcessEvent( Event *evt )
{
    switch( evt->GetMessage() )
    {
        case CTRL_ENABLED:
            Enable( (Event*)evt->GetParam1(), (bool)evt->GetParam2() );
            break;
    }
    return false;
}
//---------------------------------------------------------------------------
void ControlButton::MoveRelative( int xOff, int yOff )
{
    Left += xOff;
    Top  += yOff;
}
//---------------------------------------------------------------------------
void ControlButton::Draw( int x, int y, int w, int h, Graphics *dest )
{
    if( !Visible )
        return;

    int xI, yI, wI, hI;
    if( GetIntersectRgn( x,y,w,h,Left,Top,Width,Height, xI, yI, wI, hI ) )
    {
        // Button is in down state
        if( State == 0 && Enabled )
            Img[1]->DrawBitmap( xI-Left, yI-Top, wI, hI, xI-x, yI-y, dest );

        // Button is in up state
        if( State == 1 && Enabled )
            Img[0]->DrawBitmap( xI-Left, yI-Top, wI, hI, xI-x, yI-y, dest );

        // Button is disabled
        if( !Enabled )
            Img[2]->DrawBitmap( xI-Left, yI-Top, wI, hI, xI-x, yI-y, dest );
    }
}
//---------------------------------------------------------------------------
bool ControlButton::MouseUp( int x, int y, int button )
{
    // If hit in the button


    if( Img[1]->Hit( x - Left, y - Top ) )
    {
        if( !Enabled )
            return true;

        if( button == 1 && Selected )
        {
            State = 1;
            Selected = false;
            ClickAction->SendEvent();
            ParentWindow->Refresh( Left, Top, Width, Height );
            return true;
        }
    }

    if( button == 1 )
        Selected = false;

    return false;
}
//---------------------------------------------------------------------------
bool ControlButton::MouseDown( int x, int y, int button )
{
    if( Img[0]->Hit( x - Left, y - Top ) )
    {
        if( !Enabled )
            return true;

        if( button == 1 )
        {
            State = 0;
            Selected = true;
            ParentWindow->Refresh( Left, Top, Width, Height );
            return true;
        }
    }

    return false;
}
//---------------------------------------------------------------------------
bool ControlButton::MouseMove( int x, int y, int button )
{
    if( !Enabled )
        return false;


    if( MouseOver( x, y ) && !CursorIn )
    {
        if( button == 1 && Selected )
        {
            State = 0;
            ParentWindow->Refresh( Left, Top, Width, Height );
        }

        if( MouseOverActionName != "none" )
        {
            MouseOverAction->SendEvent();
        }

        CursorIn = true;
        return true;
    }
    else if( !MouseOver( x, y ) & CursorIn )
    {

        if( button == 1 && Selected )
        {
            State = 1;
            ParentWindow->Refresh( Left, Top, Width, Height );
        }

        if( MouseOutActionName != "none" )
        {
            MouseOutAction->SendEvent();
        }

        CursorIn = false;
        return true;
    }

    return false;
}
//---------------------------------------------------------------------------
bool ControlButton::MouseOver( int x, int y )
{
    if( Img[1 - State]->Hit( x - Left, y - Top ) )
    {
        return true;
    }
    else
    {
        return false;
    }
}
//---------------------------------------------------------------------------
bool ControlButton::ToolTipTest( int x, int y )
{
    if( MouseOver( x, y ) && Enabled )
    {
        ParentWindow->ChangeToolTipText( ToolTipText );
        return true;
    }
    return false;
}
//---------------------------------------------------------------------------
void ControlButton::Enable( Event *event, bool enabled )
{
    if( !ClickAction->MatchEvent( event, ACTION_MATCH_ONE ) )
        return;

    if( enabled != Enabled )
    {
        Enabled = enabled;

        // If cursor is in, send mouse out event
        if( !Enabled && CursorIn )
        {
            if( MouseOutActionName != "none" )
                MouseOutAction->SendEvent();
            CursorIn = false;
        }

        ParentWindow->Refresh( Left, Top, Width, Height );
    }
}
//---------------------------------------------------------------------------

