/*****************************************************************************
 * x11_window.cpp: X11 implementation of the Window class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: x11_window.cpp,v 1.27 2003/06/22 13:06:23 asmax Exp $
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
#include <X11/Xatom.h>
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


static bool ToolTipCallback( void *data );
static void DrawToolTipText( tooltip_t *tooltip );


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
    Wnd         = wnd;
    display     = p_intf->p_sys->display;
    int screen  = DefaultScreen( display );
    Name        = name;
    LButtonDown = false;
    RButtonDown = false;

    // Creation of a graphic context that doesn't generate a GraphicsExpose
    // event when using functions like XCopyArea
    XGCValues gcVal;
    gcVal.graphics_exposures = False;
    XLOCK;
    Gc = XCreateGC( display, wnd, GCGraphicsExposures, &gcVal );
    XUNLOCK;

    // Removing fading effect
    Transition  = 0;

    if( DragDrop )
    {
        // register the listview as a drop target
        DropObject = new X11DropObject( p_intf, Wnd );

        Atom xdndAtom = XInternAtom( display, "XdndAware", False );
        char xdndVersion = 4;
        XLOCK;
        XChangeProperty( display, wnd, xdndAtom, XA_ATOM, 32, 
                         PropModeReplace, (unsigned char *)&xdndVersion, 1);
        XUNLOCK;
    }

    // Associate vlc icon to the window
    XLOCK;
    XWMHints *hints = XGetWMHints( display, Wnd );
    if( !hints)
    {
        hints = XAllocWMHints();
    }
    if( p_intf->p_sys->iconPixmap != None )
    {
        hints->icon_pixmap = p_intf->p_sys->iconPixmap;
        hints->flags |= IconPixmapHint;
    }
    if( p_intf->p_sys->iconMask != None )
    {
        hints->icon_mask = p_intf->p_sys->iconMask;
        hints->flags |= IconMaskHint;
    }
    XSetWMHints( display, Wnd, hints );
    XFree( hints );
    XUNLOCK;

    // Create Tool Tip window
    XColor color;
    color.red = 0xffff;
    color.green = 0xffff;
    color.blue = 0xa000;
    Colormap cm = DefaultColormap( display, screen );
    Window root = DefaultRootWindow( display );

    XLOCK;
    XAllocColor( display, cm, &color );
    XSetWindowAttributes attr;
    attr.background_pixel = color.pixel;
    attr.override_redirect = True;
    ToolTip.window = XCreateWindow( display, root, 0, 0, 1, 1, 1, 0,
                                    InputOutput, CopyFromParent,
                                    CWBackPixel|CWOverrideRedirect, &attr );
    ToolTip.font = XLoadFont( display,
                              "-*-helvetica-bold-r-*-*-*-80-*-*-*-*-*-*" );
    gcVal.font = ToolTip.font;
    gcVal.foreground = 0;
    gcVal.background = color.pixel;
    ToolTip.gc = XCreateGC( display, ToolTip.window, 
                            GCBackground|GCForeground|GCFont, &gcVal );
    XUNLOCK;

    ToolTip.display = display;
    X11Timer *timer = new X11Timer( p_intf, 500000, ToolTipCallback, &ToolTip );
    ToolTip.p_intf = p_intf;
    ToolTip.timer = timer;
    ToolTip.active = False;

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
    if( DragDrop )
    {
        delete DropObject;
    }
    delete ToolTip.timer;
    XLOCK;
    XFreeGC( display, ToolTip.gc );
    XFreeGC( display, Gc );
    XDestroyWindow( display, ToolTip.window );
    XDestroyWindow( display, Wnd );
    XUNLOCK;
}
//---------------------------------------------------------------------------
void X11Window::OSShow( bool show )
{
    XLOCK;
    XResizeWindow( display, Wnd, 1, 1 ); // Avoid flicker
    XUNLOCK;

    if( show )
    {
        // We do the call to XShapeCombineRegion() here because the window
        // must be unmapped for this to work.
        Drawable drawable = (( X11Graphics* )Image )->GetImage();

        XLOCK;
        XImage *image = XGetImage( display, drawable, 0, 0, Width, Height, 
                                   AllPlanes, ZPixmap );
        if( image )
        {
            // Mask for transparency
            Region region = XCreateRegion();
            for( int line = 0; line < Height; line++ )
            {
                int start = 0, end = 0;
                while( start < Width )
                {
                    while( start < Width && XGetPixel( image, start, line )
                           == 0 )
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
            XDestroyImage( image );

            XShapeCombineRegion( display, Wnd, ShapeBounding, 0, 0, region,
                                 ShapeSet );
            XDestroyRegion( region );

        }
        else
        {
            msg_Err( p_intf, "X11Window::OSShow XShapeCombineRegion() failed");
        }

        XMapWindow( display, Wnd );
        XMoveResizeWindow( display, Wnd, Left, Top, Width, Height );
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
    //unsigned int p1  = evt->GetParam1();
    int          p2  = evt->GetParam2();
    int          time;
    int          posX, posY;
    string       type;
    int          button;

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
                Window id = ( (X11Window *)(*win) )->GetHandle();
                // the current window is raised last
                if( id != Wnd )
                {
                    XLOCK;
                    XRaiseWindow( display, id );
                    XUNLOCK;
                }
            }
            XLOCK;
            XRaiseWindow( display, Wnd );
            XUNLOCK;
            
            button = 0;
            if( ((XButtonEvent *)p2 )->state & ControlMask )
            {
                button |= KEY_CTRL;
            } 
            if( ((XButtonEvent *)p2 )->state & ShiftMask )
            {
                button |= KEY_SHIFT;
            }

            switch( ( (XButtonEvent *)p2 )->button )
            {
                case 1:
                    // Left button
                    button |= MOUSE_LEFT;
                    time = OSAPI_GetTime();
                    OSAPI_GetMousePos( posX, posY );
                    if( time - ClickedTime < DblClickDelay && 
                        posX == ClickedX && posY == ClickedY )
                    {
                        // Double-click
                        ClickedTime = 0; 
                        MouseDblClick( (int)( (XButtonEvent *)p2 )->x,
                                       (int)( (XButtonEvent *)p2 )->y, button );
                    }
                    else
                    {
                        ClickedTime = time;
                        ClickedX = posX;
                        ClickedY = posY;
                        LButtonDown = true;
                        MouseDown( (int)( (XButtonEvent *)p2 )->x,
                                   (int)( (XButtonEvent *)p2 )->y, button );
                    }
                    break;

                case 3:
                    // Right button
                    button |= MOUSE_RIGHT;
                    RButtonDown = true;
                    MouseDown( (int)( (XButtonEvent *)p2 )->x,
                               (int)( (XButtonEvent *)p2 )->y, button );
                    break;

                default:
                    break;
            }
            return true;

        case ButtonRelease:
            button = 0;
            if( ((XButtonEvent *)p2 )->state & ControlMask )
            {
                button |= KEY_CTRL;
            }
            if( ((XButtonEvent *)p2 )->state & ShiftMask )
            {
                button |= KEY_SHIFT;
            }

            switch( ( (XButtonEvent *)p2 )->button )
            {
                case 1:
                    // Left button
                    button |= MOUSE_LEFT;
                    LButtonDown = false;
                    MouseUp( (int)( (XButtonEvent *)p2 )->x,
                             (int)( (XButtonEvent *)p2 )->y, button );
                    break;

                case 3:
                    button |= MOUSE_RIGHT;
                    // Right button
                    RButtonDown = false;
                    MouseUp( (int)( (XButtonEvent *)p2 )->x,
                             (int)( (XButtonEvent *)p2 )->y, button );
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

        case ClientMessage:
            XLOCK;
            type = XGetAtomName( display, ( (XClientMessageEvent*)
                                             p2 )->message_type );
            XUNLOCK;
            if( type == "XdndEnter" )
            {
                DropObject->DndEnter( ((XClientMessageEvent*)p2)->data.l );
                return true;
            }
            else if( type == "XdndPosition" )
            {
                DropObject->DndPosition( ((XClientMessageEvent*)p2)->data.l );
                return true;
            }
            else if( type == "XdndLeave" )
            {
                DropObject->DndLeave( ((XClientMessageEvent*)p2)->data.l );
                return true;
            }
            else if( type == "XdndDrop" )
            {
                DropObject->DndDrop( ((XClientMessageEvent*)p2)->data.l );
                return true;
            }
            return false;
            
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
    int direction, fontAscent, fontDescent;

    Display *disp = ((tooltip_t*)data)->display;
    Window win = ((tooltip_t*)data)->window;
    Font font = ((tooltip_t*)data)->font;
    GC gc = ((tooltip_t*)data)->gc;
    string text = ((tooltip_t*)data)->text;
    int curX = ((tooltip_t*)data)->curX;
    int curY = ((tooltip_t*)data)->curY;
 
    XLOCK;
    XClearWindow( disp, win );
    XCharStruct overall;
    XQueryTextExtents( disp, font, text.c_str(), text.size(), &direction, 
                       &fontAscent, &fontDescent, &overall );
    int w = overall.rbearing - overall.lbearing;
    int h = overall.ascent + overall.descent;
    XMapRaised( disp, win );
    XMoveWindow( disp, win, curX - w/4, curY + 20 );
    XResizeWindow( disp, win, w+8, h+8 );
    XDrawString( disp, win, gc, 4, overall.ascent+4, text.c_str(), 
                 text.size() );
    XSync( disp, 0 );
    ((tooltip_t*)data)->active = True;
    XUNLOCK;
    
    return False;
}



void DrawToolTipText( tooltip_t *tooltip )
{
    int direction, fontAscent, fontDescent;

    Display *disp = tooltip->display;
    Window win = tooltip->window;
    Font font = tooltip->font;
    GC gc = tooltip->gc;
    string text = tooltip->text;
    int curX = tooltip->curX;
    int curY = tooltip->curY;
 
    XLOCK;
    XClearWindow( disp, win );
    XCharStruct overall;
    XQueryTextExtents( disp, font, text.c_str(), text.size(), &direction, 
                       &fontAscent, &fontDescent, &overall );
    int w = overall.rbearing - overall.lbearing;
    int h = overall.ascent + overall.descent;
    XMoveWindow( disp, win, curX - w/4, curY + 20 );
    XResizeWindow( disp, win, w+8, h+8 );
    XDrawString( disp, win, gc, 4, overall.ascent+4, text.c_str(), 
                 text.size() );
    XSync( disp, 0 );
    XUNLOCK;
}


void X11Window::ChangeToolTipText( string text )
{
    if( text == "none" )
    {
        if( ToolTipText != "none" )
        {
            ToolTipText = "none";
            XLOCK;
            // Hide the tooltip window
            X11TimerManager *timerManager = X11TimerManager::Instance( p_intf );
            timerManager->removeTimer( ToolTip.timer );
            XUnmapWindow( display, ToolTip.window );
            XResizeWindow( display, ToolTip.window, 1, 1 );
            XSync( display, 0 );
            ToolTip.active = False;
            XUNLOCK;
        }
    }
    else
    {
        if( text != ToolTipText )
        {
            ToolTipText = text;
            ToolTip.text = text;
            if( !ToolTip.active )
            {
                // Create the tooltip
                OSAPI_GetMousePos( ToolTip.curX, ToolTip.curY );
                X11TimerManager *timerManager = X11TimerManager::Instance( p_intf );
                timerManager->addTimer( ToolTip.timer );
            }
            else
            {
                // Refresh the tooltip
                DrawToolTipText( &ToolTip );
            }
        }
    }
}
//---------------------------------------------------------------------------

#endif
