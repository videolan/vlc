/*****************************************************************************
 * slider.cpp: Slider control
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: slider.cpp,v 1.7 2003/04/21 21:51:16 asmax Exp $
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


//--- GENERAL ---------------------------------------------------------------
#include <math.h>
#include <stdio.h>

//--- VLC -------------------------------------------------------------------
#include <vlc/intf.h>

//--- SKIN ------------------------------------------------------------------
#include "../os_api.h"
#include "../src/bitmap.h"
#include "../src/bezier.h"
#include "../src/banks.h"
#include "../src/graphics.h"
#include "../os_graphics.h"
#include "generic.h"
#include "slider.h"
#include "../src/event.h"
#include "../src/theme.h"
#include "../src/window.h"
#include "../src/skin_common.h"



//---------------------------------------------------------------------------
// Control Slider
//---------------------------------------------------------------------------
ControlSlider::ControlSlider( string id, bool visible, string type,
    string cursorUp, string cursorDown, double *ptx, double *pty, int nb,
    string tooltiptext, string help, SkinWindow *Parent )
    : GenericControl( id, visible, help, Parent )
{
    Type             = type;
    Left             = 0;
    Top              = 0;
    State            = 1;
    Value            = 0;
    Selected         = false;
    Enabled          = false;
    this->cursorUp   = cursorUp;
    this->cursorDown = cursorDown;
    Curve            = new Bezier( ptx, pty, nb );
    UpdateEvent      = NULL;
    SliderRange      = SLIDER_RANGE;
    LastRefreshTime  = OSAPI_GetTime();

    // Tool tip text
    BaseToolTipText = tooltiptext;
    FullToolTipText = BaseToolTipText;
}
//---------------------------------------------------------------------------
ControlSlider::~ControlSlider()
{
    delete[] CursorX;
    delete[] CursorY;
    delete (OSRegion *)HitRgn;
}
//---------------------------------------------------------------------------
void ControlSlider::Init()
{
    int i;
    // Get bitmap from list
    Img    = new (::Bitmap*)[2];
    Img[0] = p_intf->p_sys->p_theme->BmpBank->Get( cursorUp );
    Img[1] = p_intf->p_sys->p_theme->BmpBank->Get( cursorDown );

    // Get images sizes
    Img[0]->GetSize( CWidth, CHeight );

    // Computing slider curve : get points
    MaxValue = Curve->GetNumOfDifferentPoints();
    CursorX  = new int[MaxValue + 1];
    CursorY  = new int[MaxValue + 1];
    Curve->GetDifferentPoints( CursorX, CursorY, -CWidth / 2, -CHeight / 2 );

        // Search for size value
        Left   = CursorX[0];
        Top    = CursorY[0];
        Width  = CursorX[0];
        Height = CursorY[0];
        for( i = 1; i <= MaxValue; i++ )
        {
            if( CursorX[i] < Left )
                Left = CursorX[i];
            if( CursorY[i] < Top )
                Top  = CursorY[i];
            if( CursorX[i] > Width )
                Width = CursorX[i];
            if( CursorY[i] > Height )
                Height  = CursorY[i];
        }
        Width  += CWidth  - Left;
        Height += CHeight - Top;

        // Curve is no more needed so delete it
        delete Curve;

    // Create Hit Region
    HitRgn = (Region *)new OSRegion;

    // Create slider hit region and move cursor inside control
    for( i = 0; i <= MaxValue; i++ )
    {
        HitRgn->AddElipse( CursorX[i], CursorY[i], CWidth, CHeight );
        CursorX[i] -= Left;
        CursorY[i] -= Top;
    }

    // Select type of slider
    if( Type == "time" )
    {
        Enabled = false;
        UpdateEvent = p_intf->p_sys->p_theme->EvtBank->Get( "time" );
    }
    else if( Type == "volume" )
    {
        Enabled = true;
        UpdateEvent = p_intf->p_sys->p_theme->EvtBank->Get( "volume_refresh" );
    }
    else
    {
        Enabled = false;
        UpdateEvent = p_intf->p_sys->p_theme->EvtBank->Get( "none" );
    }
}
//---------------------------------------------------------------------------
bool ControlSlider::ProcessEvent( Event *evt )
{
    unsigned int msg = evt->GetMessage();
    unsigned int p1  = evt->GetParam1();
    int          p2  = evt->GetParam2();

    switch( msg )
    {
        case CTRL_ENABLED:
            Enable( (Event*)p1, (bool)p2 );
            return true;

        case CTRL_SET_SLIDER:
            if( UpdateEvent->IsEqual( (Event*)p1 ) )
            {
                SetCursorPosition( (long)p2 );
            }
            return true;

    }
    return false;
}
//---------------------------------------------------------------------------
void ControlSlider::MoveRelative( int xOff, int yOff )
{
    Left += xOff;
    Top  += yOff;
    HitRgn->Move( xOff, yOff );
}
//---------------------------------------------------------------------------
void ControlSlider::SetCursorPosition( long Pos )
{
    if( Pos < 0 )
        Pos = 0;
    if( Pos > SliderRange )
        Pos = SliderRange;

    if( !Selected )
    {
        if( SliderRange == 0 )
            MoveCursor( 0 );
        else
            MoveCursor( Pos * MaxValue / SliderRange );
    }
}
//---------------------------------------------------------------------------
long ControlSlider::GetCursorPosition()
{
    return SliderRange * Value / MaxValue;
}
//---------------------------------------------------------------------------
void ControlSlider::MoveCursor( int newValue )
{
    int X, Y, W, H;
    int OldValue = Value;
    Value = newValue;
    if( OldValue != Value )
    {
        X = (CursorX[Value] > CursorX[OldValue])
            ? Left + CursorX[OldValue] : Left + CursorX[Value];
        Y = (CursorY[Value] > CursorY[OldValue])
            ? Top  + CursorY[OldValue] : Top  + CursorY[Value];
        W = (CursorX[Value] > CursorX[OldValue])
            ? CursorX[Value] - CursorX[OldValue] + CWidth
            : CursorX[OldValue] - CursorX[Value] + CWidth;
        H = (CursorY[Value] > CursorY[OldValue])
            ? CursorY[Value] - CursorY[OldValue] + CHeight
            : CursorY[OldValue] - CursorY[Value] + CHeight;
        if( 2 * CWidth * CHeight < W * H )
        {
            ParentWindow->Refresh( Left + CursorX[OldValue],
                Top + CursorY[OldValue], CWidth, CHeight );
            ParentWindow->Refresh( Left + CursorX[Value],
                Top + CursorY[Value], CWidth, CHeight );
        }
        else
        {
            ParentWindow->Refresh( X, Y, W, H );
        }

        // Change tooltip
        if( BaseToolTipText != "none" )
        {
            char *percent = new char[6];
            sprintf( percent, "%i %%", Value * 100 / MaxValue );
            FullToolTipText = BaseToolTipText + " - " + (string)percent;
            delete[] percent;
        }
    }
}
//---------------------------------------------------------------------------
void ControlSlider::Draw( int x, int y, int w, int h, Graphics *dest )
{
    if( !Visible )
        return;

    int xI, yI, wI, hI;

    if( GetIntersectRgn( x, y, w, h, Left+CursorX[Value], Top+CursorY[Value],
                         CWidth, CHeight, xI, yI, wI, hI ) )
    {
        Img[1 - State]->DrawBitmap( xI - Left - CursorX[Value],
            yI - Top - CursorY[Value], wI, hI, xI - x, yI - y, dest );
    }
}
//---------------------------------------------------------------------------
bool ControlSlider::MouseUp( int x, int y, int button )
{
    State = 1;
    if( Enabled && Selected )
    {
        Selected = false;
        ParentWindow->Refresh( Left + CursorX[Value],
            Top + CursorY[Value], CWidth, CHeight );
        UpdateEvent->SetParam2( GetCursorPosition() );
        UpdateEvent->SendEvent();
    }
    return false;
}
//---------------------------------------------------------------------------
bool ControlSlider::MouseDown( int x, int y, int button )
{
    if( !Enabled )
        return false;

    // If hit into cursor or indide active slider region
    if( HitRgn->Hit( x, y ) && button == 1 )
    {
        State = 0;
        Selected = true;
        ParentWindow->Refresh( Left + CursorX[Value],
            Top + CursorY[Value], CWidth, CHeight );
        MoveCursor( FindNearestPoint( x, y ) );
        UpdateEvent->SetParam2( GetCursorPosition() );
        UpdateEvent->SendEvent();
        return true;
    }
    return false;
}
//---------------------------------------------------------------------------
bool ControlSlider::MouseMove( int x, int y, int button )
{
    if( !Enabled || !Selected || !button )
        return false;

    MoveCursor( FindNearestPoint( x, y ) );

    // Refresh value if time ellapsed since last refresh is more than 200 ms
    int time = OSAPI_GetTime();
    if( time > LastRefreshTime + 250 )
    {
        UpdateEvent->SetParam2( GetCursorPosition() );
        UpdateEvent->SendEvent();
        LastRefreshTime = time;
    }
    return true;
}
//---------------------------------------------------------------------------
bool ControlSlider::MouseOver( int x, int y )
{
    if( HitRgn->Hit( x, y ) )
        return true;
    else
        return false;
}
//---------------------------------------------------------------------------
bool ControlSlider::MouseScroll( int x, int y, int direction )
{
    if( !Enabled || !MouseOver( x, y ) )
        return false;

    int val = Value;

    switch( direction )
    {
        case MOUSE_SCROLL_DOWN:
            if( val > 0 ) val--;
            break;

        case MOUSE_SCROLL_UP:
            if( val < MaxValue ) val++;
            break;
    }

    MoveCursor( val );
    return true;
}
//---------------------------------------------------------------------------
bool ControlSlider::ToolTipTest( int x, int y )
{
    if( MouseOver( x, y ) )
    {
        if( BaseToolTipText == "none" )
        {
            ParentWindow->ChangeToolTipText( BaseToolTipText );
        }
        else
        {
            ParentWindow->ChangeToolTipText( FullToolTipText );
        }
        return true;
    }
    return false;
}
//---------------------------------------------------------------------------
void ControlSlider::ChangeSliderRange( int NewRange )
{
    if( NewRange == SliderRange )
        return;

    SliderRange = NewRange;

    int Pos = GetCursorPosition();
    SetCursorPosition( Pos );

}
//---------------------------------------------------------------------------
void ControlSlider::Enable( Event *event, bool enabled )
{
    if( !UpdateEvent->IsEqual( event ) )
        return;

    if( enabled && !Enabled )
    {
        Enabled = true;
        ParentWindow->Refresh( Left, Top, Width, Height );
    }
    else if( !enabled && Enabled )
    {
        Enabled = false;
        ParentWindow->Refresh( Left, Top, Width, Height );
    }
}
//---------------------------------------------------------------------------
int ControlSlider::FindNearestPoint( int x, int y )
{
    int i, wx, wy;
    double D;
    double minD = 50;
    int RefValue = Value;
    // Move point inside control
    OSAPI_GetMousePos( x, y );              // This is used to avoid bug with
                                            // negative values
    ParentWindow->GetPos( wx, wy );
    x += -wx - Left - CWidth  / 2;
    y += -wy - Top  - CHeight / 2;

    // Search nearest point
    for( i = 0; i <= MaxValue; i++ )
    {
        D = sqrt( ( CursorX[i] - x ) * ( CursorX[i] - x ) +
                  ( CursorY[i] - y ) * ( CursorY[i] - y ) );
        if( D < minD )
        {
            minD = D;
            RefValue = i;
        }
    }
    return RefValue;
}
//---------------------------------------------------------------------------

