/*****************************************************************************
 * rectangle.cpp: Rectanglee control
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: rectangle.cpp,v 1.4 2003/04/21 21:51:16 asmax Exp $
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
#include "../os_api.h"
#include "../src/graphics.h"
#include "../src/bitmap.h"
#include "../src/banks.h"
#include "generic.h"
#include "rectangle.h"
#include "../src/event.h"
#include "../src/theme.h"
#include "../src/window.h"
#include "../src/skin_common.h"



//---------------------------------------------------------------------------
// Control Rectangle
//---------------------------------------------------------------------------
ControlRectangle::ControlRectangle( string id, bool visible, int x, int y,
    int w, int h, int color, string event, string help, SkinWindow *Parent )
    : GenericControl( id, visible, help, Parent )
{
    Left                = x;
    Top                 = y;
    Width               = w;
    Height              = h;
    MouseDownActionName = event;
    Enabled             = true;
    Color               = OSAPI_GetNonTransparentColor( color );
}
//---------------------------------------------------------------------------
ControlRectangle::~ControlRectangle()
{
}
//---------------------------------------------------------------------------
void ControlRectangle::Init()
{
    // Create script
    MouseDownAction = new Action( p_intf, MouseDownActionName );
}
//---------------------------------------------------------------------------
bool ControlRectangle::ProcessEvent( Event *evt  )
{

    switch( evt->GetMessage() )
    {
        case CTRL_ENABLED:
            Enable( (Event*)evt->GetParam1(), (bool)evt->GetParam2() );
            break;
        default:
            break;
    }
    return false;
}
//---------------------------------------------------------------------------
void ControlRectangle::Draw( int x, int y, int w, int h, Graphics *dest )
{
    if( !Visible )
        return;

    int xI, yI, wI, hI;
    if( GetIntersectRgn(x, y, w, h, Left, Top, Width, Height, xI, yI, wI, hI ) )
    {
        dest->DrawRect( xI - x,  yI - y, wI, hI, Color );
    }
}
//---------------------------------------------------------------------------
bool ControlRectangle::MouseDown( int x, int y, int button )
{
    if( !Enabled || !MouseOver( x, y ) || button != 1 ||
        !MouseDownAction->SendEvent() )
        return false;
    return true;
}
//---------------------------------------------------------------------------
bool ControlRectangle::MouseOver( int x, int y )
{
    if( x >= Left && x <= Left + Width && y >= Top && y <= Top + Height )
        return true;
    else
        return false;
}
//---------------------------------------------------------------------------
void ControlRectangle::Enable( Event *event, bool enabled )
{
    if( !MouseDownAction->MatchEvent( event, ACTION_MATCH_ONE ) )
        return;

    if( enabled != !Enabled )
    {
        Enabled = enabled;
        ParentWindow->Refresh( Left, Top, Width, Height );
    }
}
//---------------------------------------------------------------------------


