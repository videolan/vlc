/*****************************************************************************
 * image.cpp: Image control
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: image.cpp,v 1.8 2003/06/09 12:33:16 asmax Exp $
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
#include <vlc/vlc.h>
#include <vlc/intf.h>

//--- SKIN ------------------------------------------------------------------
#include "../src/bitmap.h"
#include "../src/banks.h"
#include "generic.h"
#include "image.h"
#include "../src/event.h"
#include "../src/theme.h"
#include "../src/window.h"
#include "../src/skin_common.h"



//---------------------------------------------------------------------------
// Control Image
//---------------------------------------------------------------------------
ControlImage::ControlImage( string id, bool visible, int x, int y, string img,
    string event, string help, SkinWindow *Parent )
    : GenericControl( id, visible, help, Parent )
{
    Left                = x;
    Top                 = y;
    MouseDownActionName = event;
    Enabled             = true;
    Bg                  = img;
}
//---------------------------------------------------------------------------
ControlImage::~ControlImage()
{
    if( MouseDownAction )
    {
        delete MouseDownAction;
    }
}
//---------------------------------------------------------------------------
void ControlImage::Init()
{
    Img    = new (Bitmap *[1]);
    Img[0] = p_intf->p_sys->p_theme->BmpBank->Get( Bg );
    Img[0]->GetSize( Width, Height );

    // Create script
    MouseDownAction = new Action( p_intf, MouseDownActionName );

}
//---------------------------------------------------------------------------
bool ControlImage::ProcessEvent( Event *evt  )
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
void ControlImage::Draw( int x, int y, int w, int h, Graphics *dest )
{
    if( !Visible )
        return;

    int xI, yI, wI, hI;
    if( GetIntersectRgn(x, y, w, h, Left, Top, Width, Height, xI, yI, wI, hI ) )
        Img[0]->DrawBitmap( xI-Left, yI-Top, wI, hI, xI-x, yI-y, dest );

}
//---------------------------------------------------------------------------
bool ControlImage::MouseDown( int x, int y, int button )
{
    if( !Enabled || !Img[0]->Hit( x - Left, y - Top ) || button != 1 ||
        !MouseDownAction->SendEvent() )
            return false;

    return true;
}
//---------------------------------------------------------------------------
bool ControlImage::MouseOver( int x, int y )
{
    if( Img[0]->Hit( x - Left, y - Top ) )
        return true;
    else
        return false;
}
//---------------------------------------------------------------------------
void ControlImage::Enable( Event *event, bool enabled )
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


