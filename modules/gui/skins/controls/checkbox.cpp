/*****************************************************************************
 * checkbox.cpp: Checkbox control
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: checkbox.cpp,v 1.3 2003/04/11 21:19:49 videolan Exp $
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
#include "bitmap.h"
#include "banks.h"
#include "generic.h"
#include "checkbox.h"
#include "event.h"
#include "theme.h"
#include "window.h"
#include "skin_common.h"


#include "os_event.h"
#include "os_window.h"

//---------------------------------------------------------------------------
// Checkbox Button
//---------------------------------------------------------------------------
ControlCheckBox::ControlCheckBox(
    string id,
    bool visible,
    int x, int y,
    string img1, string img2, string clickimg1, string clickimg2,
    string disabled1, string disabled2,
    string onclick1, string onclick2, string onmouseover1,
    string onmouseout1, string onmouseover2, string onmouseout2,
    string tooltiptext1, string tooltiptext2, string help,
    Window *Parent ) : GenericControl( id, visible, help, Parent )
{
    Left             = x;
    Top              = y;
    State            = 1;                   // 1 = up - 0 = down
    Selected         = false;
    CursorIn         = false;
    Act              = 1;
    Enabled1         = true;
    Enabled2         = true;
    Img1             = img1;
    Img2             = img2;
    Click1           = clickimg1;
    Click2           = clickimg2;
    Disabled1        = disabled1;
    Disabled2        = disabled2;

    // Actions
    ClickActionName1     = onclick1;
    ClickActionName2     = onclick2;

    MouseOverActionName1 = onmouseover1;
    MouseOutActionName1  = onmouseout1;

    if( onmouseover2 == "none" )
        MouseOverActionName2 = MouseOverActionName1;
    else
        MouseOverActionName2 = onmouseover2;

    if( onmouseout2 == "none" )
        MouseOutActionName2 = MouseOutActionName1;
    else
        MouseOutActionName2  = onmouseout2;

    // Tooltips
    ToolTipText1     = tooltiptext1;
    ToolTipText2     = tooltiptext2;
}
//---------------------------------------------------------------------------
ControlCheckBox::~ControlCheckBox()
{
}
//---------------------------------------------------------------------------
void ControlCheckBox::Init()
{
    Img = new (Bitmap*)[6];

    // Images for position 1
    Img[0] = p_intf->p_sys->p_theme->BmpBank->Get( Img1 );
    if( Click1 == "none" )
        Img[1] = p_intf->p_sys->p_theme->BmpBank->Get( Img2 );
    else
        Img[1] = p_intf->p_sys->p_theme->BmpBank->Get( Click1 );

    // Images for position 2
    Img[2] = p_intf->p_sys->p_theme->BmpBank->Get( Img2 );
    if( Click2 == "none" )
        Img[3] = p_intf->p_sys->p_theme->BmpBank->Get( Img1 );
    else
        Img[3] = p_intf->p_sys->p_theme->BmpBank->Get( Click2 );

    // Disabled images
    if( Disabled1 == "none" )
        Img[4] = p_intf->p_sys->p_theme->BmpBank->Get( Img1 );
    else
        Img[4] = p_intf->p_sys->p_theme->BmpBank->Get( Disabled1 );
    if( Disabled2 == "none" )
        Img[5] = p_intf->p_sys->p_theme->BmpBank->Get( Img2 );
    else
        Img[5] = p_intf->p_sys->p_theme->BmpBank->Get( Disabled2 );

    // Get Size of control
    Img[0]->GetSize( Width, Height );

    // Create script
    ClickAction1 = new Action( p_intf, ClickActionName1 );
    ClickAction2 = new Action( p_intf, ClickActionName2 );
    MouseOverAction1 = new Action( p_intf, MouseOverActionName1 );
    MouseOutAction1  = new Action( p_intf, MouseOutActionName1 );
    MouseOverAction2 = new Action( p_intf, MouseOverActionName2 );
    MouseOutAction2  = new Action( p_intf, MouseOutActionName2 );
}
//---------------------------------------------------------------------------
bool ControlCheckBox::ProcessEvent( Event *evt  )
{
    switch( evt->GetMessage() )
    {
        case CTRL_ENABLED:
            Enable( (Event*)evt->GetParam1(), (bool)evt->GetParam2() );
            break;
        case CTRL_SYNCHRO:
            if( ClickAction1->MatchEvent( (Event*)evt->GetParam1(),
                ACTION_MATCH_ONE ) )
            {
                Act = 2;
                ParentWindow->Refresh( Left, Top, Width, Height );
            }
            else if( ClickAction2->MatchEvent( (Event*)evt->GetParam1(),
                ACTION_MATCH_ONE ) )
            {
                Act = 1;
                ParentWindow->Refresh( Left, Top, Width, Height );
            }
            break;
    }
    return false;
}
//---------------------------------------------------------------------------
void ControlCheckBox::MoveRelative( int xOff, int yOff )
{
    Left += xOff;
    Top  += yOff;
}
//---------------------------------------------------------------------------
void ControlCheckBox::Draw( int x, int y, int w, int h, Graphics *dest )
{
    if( !Visible )
        return;

    int xI, yI, wI, hI;
    if( GetIntersectRgn( x,y,w,h,Left,Top,Width,Height, xI, yI, wI, hI ) )
    {
        if( Act == 1 )
        {
            if( State == 1 && Enabled1 )
                Img[0]->DrawBitmap( xI-Left,yI-Top,wI,hI,xI-x,yI-y,dest );
            else if( State == 0 && Enabled1 )
                Img[1]->DrawBitmap( xI-Left,yI-Top,wI,hI,xI-x,yI-y,dest );
            else
                Img[4]->DrawBitmap( xI-Left,yI-Top,wI,hI,xI-x,yI-y,dest );
        }
        else if( Act == 2 )
        {
            if( State == 1 && Enabled2 )
                Img[2]->DrawBitmap( xI-Left,yI-Top,wI,hI,xI-x,yI-y,dest );
            else if( State == 0 && Enabled2 )
                Img[3]->DrawBitmap( xI-Left,yI-Top,wI,hI,xI-x,yI-y,dest );
            else
                Img[5]->DrawBitmap( xI-Left,yI-Top,wI,hI,xI-x,yI-y,dest );
        }
    }
}
//---------------------------------------------------------------------------
bool ControlCheckBox::MouseUp( int x, int y, int button )
{
    // Test enabled
    if( ( !Enabled1 && Act == 1 && Img[1]->Hit( x - Left, y - Top ) ) ||
        ( !Enabled2 && Act == 2 && Img[3]->Hit( x - Left, y - Top ) ) )
            return true;

    if( button == 1 && Selected )
    {
        if( Act == 1 && Img[1]->Hit( x - Left, y - Top ) )
        {
            State    = 1;
            Selected = false;
            Act      = 2;
            ParentWindow->Refresh( Left, Top, Width, Height );
            ClickAction1->SendEvent();
            return true;
        }
        else if( Act == 2 && Img[3]->Hit( x - Left, y - Top ) )
        {
            State    = 1;
            Selected = false;
            Act      = 1;
            ParentWindow->Refresh( Left, Top, Width, Height );
            ClickAction2->SendEvent();
            return true;
        }
    }
    Selected = false;
    return false;
}
//---------------------------------------------------------------------------
bool ControlCheckBox::MouseDown( int x, int y, int button )
{

    // Test enabled
    if( ( !Enabled1 && Act == 1 && Img[1]->Hit( x - Left, y - Top ) ) ||
        ( !Enabled2 && Act == 2 && Img[3]->Hit( x - Left, y - Top ) ) )
            return true;
    if( button == 1 )
    {
        if( Act == 1 && Img[0]->Hit( x - Left, y - Top ) )
        {
            State    = 0;
            Selected = true;
            ParentWindow->Refresh( Left, Top, Width, Height );
            return true;
        }
        else if( Act == 2 && Img[2]->Hit( x - Left, y - Top ) )
        {
            State    = 0;
            Selected = true;
            ParentWindow->Refresh( Left, Top, Width, Height );
            return true;
        }
    }
    return false;
}
//---------------------------------------------------------------------------
bool ControlCheckBox::MouseMove( int x, int y, int button )
{
    // Test enabled
    if( ( !Enabled1 && Act == 1 ) || ( !Enabled2 && Act == 2 ) )
        return false;

    // If mouse is entering control
    if( MouseOver( x, y ) && !CursorIn )
    {
        // If control is already selected
        if( button == 1 && Selected )
        {
            State = 0;
            ParentWindow->Refresh( Left, Top, Width, Height );
        }

        // Check events
        if( Act == 1 && MouseOverActionName1 != "none" )
            MouseOverAction1->SendEvent();
        else if( Act == 2 && MouseOverActionName2 != "none" )
            MouseOverAction2->SendEvent();

        CursorIn = true;
        return true;
    }
    // If mouse if leaving control
    else if( !MouseOver( x, y ) && CursorIn )
    {
        // If control is already selected
        if( button == 1 && Selected )
        {
            State = 1;
            ParentWindow->Refresh( Left, Top, Width, Height );
        }

        // Check events
        if( Act == 1 && MouseOutActionName1 != "none" )
            MouseOutAction1->SendEvent();
        else if( Act == 2 && MouseOutActionName2 != "none" )
            MouseOutAction2->SendEvent();

        CursorIn = false;
        return true;
    }
    return true;
}
//---------------------------------------------------------------------------
bool ControlCheckBox::MouseOver( int x, int y )
{
    if( Act == 1 )
    {
        if( Img[1 - State]->Hit( x - Left, y - Top ) )
            return true;
    }
    else if( Act == 2 )
    {
        if( Img[3 - State]->Hit( x - Left, y - Top ) )
            return true;
    }
    return false;
}
//---------------------------------------------------------------------------
bool ControlCheckBox::ToolTipTest( int x, int y )
{
    if( Act == 1 && MouseOver( x, y ) && Enabled1 )
    {
        ParentWindow->ChangeToolTipText( ToolTipText1 );
        return true;
    }
    else if( Act == 2 && MouseOver( x, y ) && Enabled2 )
    {
        ParentWindow->ChangeToolTipText( ToolTipText2 );
        return true;
    }
    return false;
}
//---------------------------------------------------------------------------
void ControlCheckBox::Enable( Event *event, bool enabled )
{
    if( enabled != !Enabled1 &&
        ClickAction1->MatchEvent( event, ACTION_MATCH_ONE ) )
    {
        Enabled1 = enabled;
        if( Act == 1 )
        {
            // If cursor is in, send mouse out event
            if( !Enabled1 && CursorIn )
            {
                if( MouseOutActionName2 != "none" )
                    MouseOutAction2->SendEvent();
                CursorIn = false;
            }
            ParentWindow->Refresh( Left, Top, Width, Height );
        }
    }



    else if( enabled != !Enabled2 &&
        ClickAction2->MatchEvent( event, ACTION_MATCH_ONE ) )
    {
        Enabled2 = enabled;
        if( Act == 2 )
        {
            // If cursor is in, send mouse out event
            if( !Enabled2 && CursorIn )
            {
                if( MouseOutActionName2 != "none" )
                    MouseOutAction2->SendEvent();
                CursorIn = false;
            }
            ParentWindow->Refresh( Left, Top, Width, Height );
        }
    }

}
//---------------------------------------------------------------------------

