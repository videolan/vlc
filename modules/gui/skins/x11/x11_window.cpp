/*****************************************************************************
 * x11_window.cpp: X11 implementation of the Window class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: x11_window.cpp,v 1.12 2003/06/07 12:19:23 asmax Exp $
 *
 * Authors: Cyril Deguet     <asmax@videolan.org>
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

#ifdef X11_SKINS

//--- GENERAL ---------------------------------------------------------------
//#include <math.h>

//--- VLC -------------------------------------------------------------------
#include <vlc/intf.h>

//--- X11 -------------------------------------------------------------------
#include <X11/Xlib.h>
#include <X11/extensions/shape.h>

//--- SKIN ------------------------------------------------------------------
#include "../os_api.h"
#include "../src/anchor.h"
#include "../controls/generic.h"
#include "../src/window.h"
#include "../os_window.h"
#include "../src/event.h"
#include "../os_event.h"
#include "../src/graphics.h"
#include "../os_graphics.h"
#include "../src/skin_common.h"
#include "../src/theme.h"
#include "../os_theme.h"
#include "x11_timer.h"


bool ToolTipCallback( void *data );


//---------------------------------------------------------------------------
// Skinable Window
//---------------------------------------------------------------------------
X11Window::X11Window( intf_thread_t *p_intf, Window wnd, int x, int y,
    bool visible, int transition, int normalalpha, int movealpha,
    bool dragdrop, string name )
    : SkinWindow( p_intf, x, y, visible, transition, normalalpha, movealpha,
              dragdrop )
{
    // Set handles
    Wnd           = wnd;

    display = p_intf->p_sys->display;
    int screen = DefaultScreen( display );

    Gc = DefaultGC( display, screen );

    Name        = name;

    LButtonDown = false;
    RButtonDown = false;

    // Removing fading effect
    Transition  = 0;
/*
    // Set position parameters
    CursorPos    = new POINT;
    WindowPos    = new POINT;

*/

    if( DragDrop )
    {
        // register the listview as a drop target
        DropObject = new X11DropObject( p_intf );
    //    gdk_window_register_dnd( gwnd );
    }

    // Create Tool Tip window
    ToolTipWindow = XCreateSimpleWindow( display, wnd, 0, 0, 1, 1, 0, 0, 0 );
    X11Timer *timer = new X11Timer( p_intf, 100, ToolTipCallback, &ToolTipInfo );
    ToolTipInfo.p_intf = p_intf;
    ToolTipInfo.timer = timer;

    // Double-click handling
    ClickedX = 0;
    ClickedY = 0;
    ClickedTime = 0;
    // TODO: can be retrieved somewhere ?
    DblClickDelay = 400;
   
}
//---------------------------------------------------------------------------
X11Window::~X11Window()
{
/*    delete CursorPos;
    delete WindowPos;

    if( hWnd != NULL )
    {
        DestroyWindow( hWnd );
    }*/
    XDestroyWindow( display, ToolTipWindow );
    /*
    if( DragDrop )
    {
        // Remove the listview from the list of drop targets
        RevokeDragDrop( hWnd );
        DropTarget->Release();
        // Uninitialize the OLE library
        OleUninitialize();
    }*/
 /*   if( gWnd )
    {
        gdk_window_destroy( gWnd );
    }*/
}
//---------------------------------------------------------------------------
void X11Window::OSShow( bool show )
{
    if( show )
    {
        XLOCK;
        XMapWindow( display, Wnd );
        XMoveWindow( display, Wnd, Left, Top );
        XUNLOCK;
    }
    else
    {
        XLOCK;
        XUnmapWindow( display, Wnd );
        XUNLOCK;
    }
}
//---------------------------------------------------------------------------
bool X11Window::ProcessOSEvent( Event *evt )
{
    unsigned int msg = evt->GetMessage();
    unsigned int p1  = evt->GetParam1();
    int          p2  = evt->GetParam2();
    int          time;
    int          posX, posY;

    switch( msg )
    {
        case Expose:
            RefreshFromImage( 0, 0, Width, Height );
            return true;

        case MotionNotify:
            if( LButtonDown )
                MouseMove( (int)( (XMotionEvent *)p2 )->x,
                           (int)( (XMotionEvent *)p2 )->y, 1 );
            else if( RButtonDown )
                MouseMove( (int)( (XMotionEvent *)p2 )->x,
                           (int)( (XMotionEvent *)p2 )->y, 2 );
            else
                MouseMove( (int)( (XMotionEvent *)p2 )->x,
                           (int)( (XMotionEvent *)p2 )->y, 0 );
            return true;


        case ButtonPress:
            // Raise all the windows
            for( list<SkinWindow *>::const_iterator win = 
                    p_intf->p_sys->p_theme->WindowList.begin();
                    win != p_intf->p_sys->p_theme->WindowList.end(); win++ )
            {
                XLOCK;
                XRaiseWindow( display, ( (X11Window *)(*win) )->GetHandle() );
                XUNLOCK;
            }

            switch( ( (XButtonEvent *)p2 )->button )
            {
                case 1:
                    // Left button
                    time = OSAPI_GetTime();
                    OSAPI_GetMousePos( posX, posY );
                    if( time - ClickedTime < DblClickDelay && 
                        posX == ClickedX && posY == ClickedY )
                    {
                        // Double-click
                        ClickedTime = 0; 
                        MouseDblClick( (int)( (XButtonEvent *)p2 )->x,
                                       (int)( (XButtonEvent *)p2 )->y, 1 );
                    }
                    else
                    {
                        ClickedTime = time;
                        ClickedX = posX;
                        ClickedY = posY;
                        LButtonDown = true;
                        MouseDown( (int)( (XButtonEvent *)p2 )->x,
                                   (int)( (XButtonEvent *)p2 )->y, 1 );
                    }
                    break;

                case 3:
                    // Right button
                    RButtonDown = true;
                    MouseDown( (int)( (XButtonEvent *)p2 )->x,
                               (int)( (XButtonEvent *)p2 )->y, 2 );
                    break;

                default:
                    break;
            }
            return true;

        case ButtonRelease:
            switch( ( (XButtonEvent *)p2 )->button )
            {
                case 1:
                    // Left button
                    LButtonDown = false;
                    MouseUp( (int)( (XButtonEvent *)p2 )->x,
                             (int)( (XButtonEvent *)p2 )->y, 1 );
                    break;

                case 3:
                    // Right button
                    RButtonDown = false;
                    MouseUp( (int)( (XButtonEvent *)p2 )->x,
                             (int)( (XButtonEvent *)p2 )->y, 2 );
                    break; 
                    
                case 4:
                    // Scroll up
                    MouseScroll( (int)( (XButtonEvent *)p2 )->x,
                                 (int)( (XButtonEvent *)p2 )->y,
                                 MOUSE_SCROLL_UP);
                    break;
                    
                case 5:
                    // Scroll down
                    MouseScroll( (int)( (XButtonEvent *)p2 )->x,
                                 (int)( (XButtonEvent *)p2 )->y,
                                 MOUSE_SCROLL_DOWN);
                    break;

                default:
                    break;
            }
            return true;

        case LeaveNotify:
            OSAPI_PostMessage( this, WINDOW_LEAVE, 0, 0 );
            return true;

/*        case GDK_DROP_START:
            DropObject->HandleDropStart( ( (GdkEventDND *)p2 )->context );
            return true;
*/
        default:
            return false;
    }
}
//---------------------------------------------------------------------------
void X11Window::SetTransparency( int Value )
{
/*    if( Value > -1 )
        Alpha = Value;
    SetLayeredWindowAttributes( hWnd, 0, Alpha, LWA_ALPHA | LWA_COLORKEY );
    UpdateWindow( hWnd );*/
}
//---------------------------------------------------------------------------
void X11Window::RefreshFromImage( int x, int y, int w, int h )
{
    Drawable drawable = (( X11Graphics* )Image )->GetImage();
    
    XLOCK;
    XCopyArea( display, drawable, Wnd, Gc, x, y, w, h, x, y );
 
    XImage *image = XGetImage( display, drawable, 0, 0, Width, Height, 
                               AllPlanes, ZPixmap );
 
    // Mask for transparency
    Region region = XCreateRegion();
    for( int line = 0; line < Height; line++ )
    {
        int start = 0, end = 0;
        while( start < Width )
        {
            while( start < Width && XGetPixel( image, start, line ) == 0 )
            {
                start++;
            } 
            end = start;
            while( end < Width && XGetPixel( image, end, line ) != 0)
            {
                end++;
            }
            XRectangle rect;
            rect.x = start;
            rect.y = line;
            rect.width = end - start + 1;
            rect.height = 1;
            Region newRegion = XCreateRegion();
            XUnionRectWithRegion( &rect, region, newRegion );
            XDestroyRegion( region );
            region = newRegion;
            start = end + 1;
        }
    }
    XShapeCombineRegion( display, Wnd, ShapeBounding, 0, 0, region, ShapeSet );
    XDestroyRegion( region );

    XSync( display, 0);
    XUNLOCK;
}
//---------------------------------------------------------------------------
void X11Window::WindowManualMove()
{
    // Get mouse cursor position
    int x, y;
    OSAPI_GetMousePos( x, y );

    // Move window and chek for magnetism
    p_intf->p_sys->p_theme->MoveSkinMagnet( this,
        WindowX + x - CursorX, WindowY + y - CursorY );

}
//---------------------------------------------------------------------------
void X11Window::WindowManualMoveInit()
{
    OSAPI_GetMousePos( CursorX, CursorY );
    WindowX = Left;
    WindowY = Top;
}
//---------------------------------------------------------------------------
void X11Window::Move( int left, int top )
{
    Left = left;
    Top  = top;
    XMoveWindow( display, Wnd, left, top );
}
//---------------------------------------------------------------------------
void X11Window::Size( int width, int height )
{
    Width  = width;
    Height = height;
    XResizeWindow( display, Wnd, width, height );
}
//---------------------------------------------------------------------------


bool ToolTipCallback( void *data )
{
    fprintf(stderr," TOOLTIP: %s\n", ((tooltip_t*)data)->text.c_str());
    return False;
}


void X11Window::ChangeToolTipText( string text )
{
    if( text == "none" )
    {
        if( ToolTipText != "none" )
        {
            ToolTipText = "none";
//            ToolTipInfo.lpszText = NULL;
 //           SendMessage( ToolTipWindow, TTM_ACTIVATE, 0 , 0 );
        }
    }
    else
    {
        if( text != ToolTipText )
        {
            ToolTipText = text;
            ToolTipInfo.text = text;
            X11TimerManager *timerManager = X11TimerManager::Instance( p_intf );
            timerManager->addTimer( ToolTipInfo.timer );
  //          ToolTipInfo.lpszText = (char *)ToolTipText.c_str();
        }
    }
}
//---------------------------------------------------------------------------

#endif
