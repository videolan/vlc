/*****************************************************************************
 * window.cpp: Window class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: window.cpp,v 1.8 2003/04/14 20:07:49 asmax Exp $
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
#include "anchor.h"
#include "generic.h"
#include "window.h"
#include "event.h"
#include "os_api.h"
#include "graphics.h"
#include "os_graphics.h"
#include "banks.h"
#include "theme.h"
#include "skin_common.h"



//---------------------------------------------------------------------------
// Skinable Window
//---------------------------------------------------------------------------
Window::Window( intf_thread_t *_p_intf, int x, int y, bool visible,
    int transition, int normalalpha, int movealpha, bool dragdrop )
{
    p_intf = _p_intf;

    // Set position parameters
    Left         = x;
    Top          = y;
    Width        = 0;
    Height       = 0;
    WindowMoving = false;
    Moved        = false;

    // Set transparency
    Transition   = transition;
    if( Transition < 1 )
        Transition = 1;
    NormalAlpha  = normalalpha;
    MoveAlpha    = movealpha;
    Alpha        = normalalpha;
    StartAlpha   = 0;
    EndAlpha     = 0;
    StartTime    = 0;
    EndTime      = 0;
    Lock         = 0;

    // Visible parameters
    Image               = NULL;
    Hidden              = true;
    Changing            = false;
    OnStartThemeVisible = visible;

    // Drag & drop
    DragDrop = dragdrop;

    // ToolTip
    ToolTipText = "none";
}
//---------------------------------------------------------------------------
Window::~Window()
{
    // Destroy the controls
    for( unsigned int i = 0; i < ControlList.size(); i++ )
        delete ControlList[i];
}
//---------------------------------------------------------------------------
void Window::Open()
{
    if( !Hidden )
        return;

    Changing = true;

    if( Transition )
    {
        SetTransparency( 0 );
        OSAPI_PostMessage( this, WINDOW_SHOW, 0, 0 );
        Fade( NormalAlpha, Transition );
    }
    else
    {
        OSAPI_PostMessage( this, WINDOW_SHOW, 0, 0 );
    }
}
//---------------------------------------------------------------------------
void Window::Close()
{
    Changing = true;

    if( Transition )
        Fade( 0, Transition, WINDOW_HIDE );
    else
        OSAPI_PostMessage( this, WINDOW_FADE, WINDOW_HIDE, 1242 );
}
//---------------------------------------------------------------------------
void Window::Show()
{
    Changing = false;
    Hidden   = false;
    OSShow( true );
}
//---------------------------------------------------------------------------
void Window::Hide()
{
    if( Hidden )
        return;

    Changing = false;
    Hidden   = true;
    OSShow( false );
    OSAPI_PostMessage( NULL, VLC_TEST_ALL_CLOSED, 0, 0 );
}
//---------------------------------------------------------------------------
void Window::Fade( int To, int Time, unsigned int evt )
{
    StartAlpha = Alpha;
    EndAlpha   = To;
    StartTime  = OSAPI_GetTime();
    EndTime    = StartTime + Time;
    Lock++;

    OSAPI_PostMessage( this, WINDOW_FADE, evt, Lock );
}
//---------------------------------------------------------------------------
bool Window::ProcessEvent( Event *evt )
{
    unsigned int i;
    unsigned int msg = evt->GetMessage();
    unsigned int p1  = evt->GetParam1();
    int          p2  = evt->GetParam2();

    // Send event to control if necessary
    if( msg > VLC_CONTROL )
    {
        for( i = 0; i < ControlList.size(); i++ )
            ControlList[i]->GenericProcessEvent( evt );
        return true;
    }

    // Message processing
    switch( msg )
    {
        case WINDOW_FADE:
            if( Lock == p2 && ChangeAlpha( OSAPI_GetTime() ) )
            {
                OSAPI_PostMessage( this, WINDOW_FADE, p1, p2 );
            }
            else
            {
                OSAPI_PostMessage( this, p1, 0, 0 );
            }
            return true;

        case WINDOW_MOVE:
            WindowManualMoveInit();
            WindowMoving = true;
            if( MoveAlpha )
                Fade( MoveAlpha, 100 );
            return true;

        case WINDOW_OPEN:
            switch( p1 )
            {
                case 0:
                    Close();
                    break;
                case 1:
                    Open();
                    break;
                case 2:
                    if( Hidden )
                        Open();
                    else
                        Close();
                    break;
            }
            return true;

        case WINDOW_CLOSE:
            switch( p1 )
            {
                case 0:
                    Open();
                    break;
                case 1:
                    Close();
                    break;
                case 2:
                    if( Hidden )
                        Open();
                    else
                        Close();
                    break;
            }
            return true;

        case WINDOW_SHOW:
            Show();
            return true;

        case WINDOW_HIDE:
            Hide();
            return true;

        case WINDOW_LEAVE:
            MouseMove( -1, -1, 0 );
            return true;

        case WINDOW_REFRESH:
            RefreshAll();
            return true;

        default:
            // OS specific messages processing
            return ProcessOSEvent( evt );
    }
}
//---------------------------------------------------------------------------
bool Window::ChangeAlpha( int time )
{
    if( time >= EndTime )
    {
        if( Lock )
        {
            SetTransparency( EndAlpha );
            Lock = 0;
        }
        return false;
    }

    int NewAlpha = StartAlpha + (EndAlpha - StartAlpha) * (time - StartTime)
        / (EndTime - StartTime);
    if( NewAlpha != Alpha )
        SetTransparency( NewAlpha );
    if( NewAlpha == EndAlpha )
    {
        Lock = 0;
        return false;
    }
    return true;
}
//---------------------------------------------------------------------------
void Window::RefreshImage( int x, int y, int w, int h )
{
    unsigned int i;

    // Create Bitmap Buffer
    Graphics *Buffer = (Graphics *)new OSGraphics( w, h, this );

    // Draw every control
        for( i = 0; i < ControlList.size(); i++ )
            ControlList[i]->Draw( x, y, w, h, Buffer );

    // Copy buffer in Image
    Image->CopyFrom( x, y, w, h, Buffer, 0, 0, SRC_COPY );

    // Free memory
    delete Buffer;
}
//---------------------------------------------------------------------------
void Window::Refresh( int x, int y, int w, int h )
{
    if( Image == NULL )
        return;

    // Refresh buffer image
    RefreshImage( x, y, w, h );

    if( Hidden )
        return;

    // And copy buffer to window
    RefreshFromImage( x, y, w, h );

}
//---------------------------------------------------------------------------
void Window::RefreshAll()
{
    Refresh( 0, 0, Width, Height );
}
//---------------------------------------------------------------------------
void Window::MouseDown( int x, int y, int button )
{
    // Checking event in controls

    for( int i = ControlList.size() - 1; i >= 0 ; i-- )
    {
        if( ControlList[i]->MouseDown( x, y, button ) )
        {
            return;
        }
    }
}
//---------------------------------------------------------------------------
void Window::MouseMove( int x, int y, int button  )
{
    int i;

    // Move window if selected !
    if( WindowMoving )
    {
        WindowManualMove();
    }

    // Checking event in controls
    for( i = ControlList.size() - 1; i >= 0 ; i-- )
    {
        ControlList[i]->MouseMove( x, y, button );
    }

    // Checking help text
    for( i = ControlList.size() - 1; i >= 0 ; i-- )
    {
        if( ControlList[i]->MouseOver( x, y ) )
        {
            if( ControlList[i]->SendNewHelpText() )
            {
                break;
            }
        }
    }

    // If help text not found, change it to ""
    if( i == -1 )
    {
        p_intf->p_sys->p_theme->EvtBank->Get( "help" )
            ->PostTextMessage( " " );
    }

    // Checking for change in Tool Tip
    for( i = ControlList.size() - 1; i >= 0 ; i-- )
    {
        if( ControlList[i]->ToolTipTest( x, y ) )
            break;
    }

    // If no change, delete tooltip text
    if( i == -1 )
        ChangeToolTipText( "none" );
}
//---------------------------------------------------------------------------
void Window::MouseUp( int x, int y, int button )
{
    int i;

    // Move window if selected !
    if( WindowMoving )
    {
        if( MoveAlpha )
            Fade( NormalAlpha, 100 );

        // Check for magnetism
        p_intf->p_sys->p_theme->CheckAnchors();

        WindowMoving = false;
    }

    // Checking event in controls
    for( i = ControlList.size() - 1; i >= 0 ; i-- )
    {
        if( ControlList[i]->MouseUp( x, y, button ) )
            return;
    }
}
//---------------------------------------------------------------------------
void Window::MouseDblClick( int x, int y, int button )
{
    int i;

    // Checking event in controls
    for( i = ControlList.size() - 1; i >= 0 ; i-- )
    {
        if( ControlList[i]->MouseDblClick( x, y, button ) )
            return;
    }
}
//---------------------------------------------------------------------------
void Window::Init()
{
    // Get size of window
    ReSize();

    // Refresh Image buffer
    RefreshImage( 0, 0, Width, Height );

fprintf(stderr, "kludge in window.cpp!\n");
    RefreshFromImage( 0, 0, Width, Height );
    // Move window as it hasn't been moved yet
    Move( Left, Top );
}
//---------------------------------------------------------------------------
void Window::ReSize()
{
    // Initialization
    unsigned int i;
    int w    = 0;
    int h    = 0;
    int MinX = 10000000;
    int MinY = 10000000;

    // Search size of window and negative values to move all
    for( i = 0; i < ControlList.size(); i++ )
    {
#define min(a,b) ((a)<(b))?(a):(b)
#define max(a,b) ((a)>(b))?(a):(b)
        w    = max( w,    ControlList[i]->Left + ControlList[i]->Width );
        h    = max( h,    ControlList[i]->Top + ControlList[i]->Height );
        MinX = min( MinX, ControlList[i]->Left );
        MinY = min( MinY, ControlList[i]->Top );
#undef max
#undef min
    }

    // Correct values
    w = w - MinX;
    h = h - MinY;
    if( w <= 0 )
        w = 1;
    if( h <= 0 )
        h = 1;

    // Move window and controls !
    if( MinX != 0 || MinY != 0 )
    {
        Move( Left + MinX, Top + MinY );
        for( i = 0; i < ControlList.size(); i++ )
            ControlList[i]->MoveRelative( -MinX, -MinY );
    }

    // Create buffer image for repainting if size has changed
    if( w != Width || h != Height )
    {
        // Change image buffer
        if( Image != NULL )
            delete (OSGraphics *)Image;
        Image = (Graphics *)new OSGraphics( w, h, this );


        Size( w, h );
    }

}
//---------------------------------------------------------------------------
void Window::GetSize( int &w, int &h )
{
    w = Width;
    h = Height;
}
//---------------------------------------------------------------------------
void Window::GetPos( int &x, int &y )
{
    x = Left;
    y = Top;
}
//---------------------------------------------------------------------------

