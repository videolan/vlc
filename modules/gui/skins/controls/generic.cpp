/*****************************************************************************
 * generic.cpp: Generic control, parent of the others
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: generic.cpp,v 1.2 2003/04/16 21:40:07 ipkiss Exp $
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
#include "../src/bitmap.h"
#include "../os_bitmap.h"
#include "../src/banks.h"
#include "../src/graphics.h"
#include "../os_graphics.h"
#include "../src/event.h"
#include "generic.h"
#include "../src/window.h"
#include "../src/theme.h"
#include "../src/skin_common.h"


//---------------------------------------------------------------------------
// Generic Control
//---------------------------------------------------------------------------
GenericControl::GenericControl( string id, bool visible, string help,
                                Window *Parent )
{
    ID      = id;
    Visible = visible;
    Help    = help;
    ParentWindow = Parent;
    Img     = NULL;
    p_intf  = Parent->GetIntf();
}
//---------------------------------------------------------------------------
GenericControl::~GenericControl()
{
    if( Img != NULL )
        delete Img;
}
//---------------------------------------------------------------------------
bool GenericControl::GenericProcessEvent( Event *evt )
{
    switch( evt->GetMessage() )
    {
        case CTRL_ID_VISIBLE:
            if( (GenericControl *)evt->GetParam1() == this )
            {
                if( ( evt->GetParam2() == 0 && Visible ) ||
                    ( evt->GetParam2() == 1 && !Visible ) ||
                    ( evt->GetParam2() == 2 ) )
                {
                    Visible = !Visible;
                    ParentWindow->Refresh( Left, Top, Width, Height );
                }
            }
            return false;

        case CTRL_ID_MOVE:
            if( (GenericControl *)evt->GetParam1() == this )
            {
                int x = evt->GetParam2() & 0x7FFF;
                int y = evt->GetParam2() >> 16 & 0x7FFF;
                if( evt->GetParam2() & 0x8000 )
                    x = -x;
                if( evt->GetParam2() >> 16 & 0x8000 )
                    y = -y;
                MoveRelative( x, y );
                ParentWindow->ReSize();
                ParentWindow->RefreshAll();
            }
            return false;

        default:
            return ProcessEvent( evt );
    }

}
//---------------------------------------------------------------------------
bool GenericControl::IsID( string id )
{
    if( ID == "none" || ID != id )
    {
        return false;
    }
    else
    {
        return true;
    }
}
//---------------------------------------------------------------------------
void GenericControl::Init()
{
}
//---------------------------------------------------------------------------
bool GenericControl::ProcessEvent( Event *evt )
{
    return false;
}
//---------------------------------------------------------------------------
bool GenericControl::MouseUp( int x, int y, int button )
{
    return false;
}
//---------------------------------------------------------------------------
bool GenericControl::MouseDown( int x, int y, int button )
{
    return false;
}
//---------------------------------------------------------------------------
bool GenericControl::MouseMove( int x, int y, int button )
{
    return false;
}
//---------------------------------------------------------------------------
bool GenericControl::MouseOver( int x, int y )
{
    return false;
}
//---------------------------------------------------------------------------
bool GenericControl::MouseDblClick( int x, int y, int button )
{
    return false;
}
//---------------------------------------------------------------------------
bool GenericControl::SendNewHelpText()
{
    if( Help != "" )
    {
        p_intf->p_sys->p_theme->EvtBank->Get( "help" )
            ->PostTextMessage( Help );
        return true;
    }
    return false;
}
//---------------------------------------------------------------------------
bool GenericControl::ToolTipTest( int x, int y )
{
    return false;
}
//---------------------------------------------------------------------------
void GenericControl::Enable( Event *event, bool enabled )
{
}
//---------------------------------------------------------------------------
bool GenericControl::GetIntersectRgn( int x1, int y1, int w1, int h1, int x2,
    int y2, int w2, int h2, int &x, int &y, int &w, int &h )
{
    if( x1 < x2 )       {x = x2;}      else {x = x1;}
    if( y1 < y2 )       {y = y2;}      else {y = y1;}
    if( x1+w1 < x2+w2 ) {w = x1+w1-x;} else {w = x2+w2-x;}
    if( y1+h1 < y2+h2 ) {h = y1+h1-y;} else {h = y2+h2-y;}
    return (w > 0 && h > 0);
}
//---------------------------------------------------------------------------
void GenericControl::Move( int left, int top )
{
    MoveRelative( left - Left, top - Top );
}
//---------------------------------------------------------------------------
void GenericControl::MoveRelative( int xOff, int yOff )
{
    Left += xOff;
    Top  += yOff;
}
//---------------------------------------------------------------------------
Region *GenericControl::CreateRegionFromBmp( Bitmap *bmp, int MoveX, int MoveY )
{
    // Initialization
        Region *Buffer;
        int w, h;
        int x = 0, y = 0, x_first = 0;
        bmp->GetSize( w, h );

        Buffer = (Region *)new OSRegion;

    // Parse bitmap
        for( y = 0; y < h; y++ )
        {
            for( x = 0; x < w; x++ )
            {

                if( bmp->GetBmpPixel( x, y ) == bmp->GetAlphaColor() )
                {
                    if( x_first != x )
                    {
                        Buffer->AddRectangle( x_first + MoveX, y + MoveY,
                                              x + MoveX, y + 1 + MoveY );
                    }
                    x_first = x + 1;
                }
            }
            if( x_first != w )
            {
                Buffer->AddRectangle( x_first + MoveX, y + MoveY,
                                        w + MoveX, y + 1 + MoveY );
            }
            x_first = 0;
        }
    // End of parsing
    return Buffer;
}
//---------------------------------------------------------------------------

