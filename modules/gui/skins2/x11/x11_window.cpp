/*****************************************************************************
 * x11_window.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef X11_SKINS

#include <X11/Xatom.h>

#include "../src/generic_window.hpp"
#include "../src/vlcproc.hpp"
#include "../src/vout_manager.hpp"
#include "x11_window.hpp"
#include "x11_display.hpp"
#include "x11_graphics.hpp"
#include "x11_dragdrop.hpp"
#include "x11_factory.hpp"

#include <assert.h>
#include <limits.h>

#include <new>

X11Window::X11Window( intf_thread_t *pIntf, GenericWindow &rWindow,
                      X11Display &rDisplay, bool dragDrop, bool playOnDrop,
                      X11Window *pParentWindow, GenericWindow::WindowType_t type ):
    OSWindow( pIntf ), m_rDisplay( rDisplay ), m_pParent( pParentWindow ),
    m_dragDrop( dragDrop ), m_pDropTarget( NULL ), m_type ( type )
{
    XSetWindowAttributes attr;
    unsigned long valuemask;
    std::string name_type;

    if( type == GenericWindow::FullscreenWindow )
    {
        m_wnd_parent = DefaultRootWindow( XDISPLAY );

        int i_screen = DefaultScreen( XDISPLAY );

        attr.event_mask = ExposureMask | StructureNotifyMask;
        attr.background_pixel = BlackPixel( XDISPLAY, i_screen );
        attr.backing_store = Always;
        valuemask = CWBackingStore | CWBackPixel | CWEventMask;

        if( NET_WM_STATE_FULLSCREEN == None )
        {
            attr.override_redirect = True;
            valuemask = valuemask | CWOverrideRedirect;
        }

        name_type = "Fullscreen";
    }
    else if( type == GenericWindow::VoutWindow )
    {
        m_wnd_parent = pParentWindow->m_wnd;

        int i_screen = DefaultScreen( XDISPLAY );

        attr.event_mask = ExposureMask | StructureNotifyMask;
        attr.backing_store = Always;
        attr.background_pixel = BlackPixel( XDISPLAY, i_screen );
        valuemask = CWBackingStore | CWBackPixel | CWEventMask;

        name_type = "VoutWindow";
    }
    else if( type == GenericWindow::FscWindow )
    {
        m_wnd_parent = DefaultRootWindow( XDISPLAY );

        attr.event_mask = ExposureMask | StructureNotifyMask;
        valuemask = CWEventMask;

        name_type = "FscWindow";
    }
    else
    {
        m_wnd_parent = DefaultRootWindow( XDISPLAY );

        attr.event_mask = ExposureMask | StructureNotifyMask;
        valuemask = CWEventMask;

        name_type = "TopWindow";
    }

    // Create the window
    m_wnd = XCreateWindow( XDISPLAY, m_wnd_parent, -10, 0, 10, 10, 0, 0,
                           InputOutput, CopyFromParent, valuemask, &attr );

    // wait for X server to process the previous commands
    XSync( XDISPLAY, false );

    // Set the colormap for 8bpp mode
    if( XPIXELSIZE == 1 )
    {
        XSetWindowColormap( XDISPLAY, m_wnd, m_rDisplay.getColormap() );
    }

    // Select events received by the window
    long event_mask = ExposureMask|KeyPressMask|
                      PointerMotionMask|ButtonPressMask|ButtonReleaseMask|
                      LeaveWindowMask|FocusChangeMask;
    XSelectInput( XDISPLAY, m_wnd, event_mask );

    // Store a pointer on the generic window in a map
    X11Factory *pFactory = (X11Factory*)X11Factory::instance( getIntf() );
    pFactory->m_windowMap[m_wnd] = &rWindow;

    // Changing decorations
    struct {
        unsigned long flags;
        unsigned long functions;
        unsigned long decorations;
        signed   long input_mode;
        unsigned long status;
    } motifWmHints;
    Atom hints_atom = XInternAtom( XDISPLAY, "_MOTIF_WM_HINTS", False );
    motifWmHints.flags = 2;    // MWM_HINTS_DECORATIONS;
    motifWmHints.decorations = 0;
    XChangeProperty( XDISPLAY, m_wnd, hints_atom, hints_atom, 32,
                     PropModeReplace, (unsigned char *)&motifWmHints,
                     sizeof( motifWmHints ) / sizeof( uint32_t ) );

    // Drag & drop
    if( m_dragDrop )
    {
        // Create a Dnd object for this window
        m_pDropTarget = new X11DragDrop( getIntf(), m_rDisplay, m_wnd,
                                         playOnDrop, &rWindow );

        // Register the window as a drop target
        Atom xdndAtom = XInternAtom( XDISPLAY, "XdndAware", False );
        char xdndVersion = 5;
        XChangeProperty( XDISPLAY, m_wnd, xdndAtom, XA_ATOM, 32,
                         PropModeReplace, (unsigned char *)&xdndVersion, 1 );

        // Store a pointer to be used in X11Loop
        pFactory->m_dndMap[m_wnd] = m_pDropTarget;
    }

    // Change the window title
    std::string name_window = "VLC (" + name_type + ")";
    XStoreName( XDISPLAY, m_wnd, name_window.c_str() );

    // Set the WM_TRANSIENT_FOR property
    if( type == GenericWindow::FscWindow )
    {
        // Associate the fsc window to the fullscreen window
        VoutManager* pVoutManager = VoutManager::instance( getIntf() );
        GenericWindow* pGenericWin = pVoutManager->getVoutMainWindow();
        X11Window *pWin = (X11Window*)pGenericWin->getOSWindow();
        Window wnd = pWin->getDrawable();
        XSetTransientForHint( XDISPLAY, m_wnd, wnd );
    }
    else
    {
        // Associate the regular top-level window to the offscren main window
        XSetTransientForHint( XDISPLAY, m_wnd, m_rDisplay.getMainWindow() );
    }

    // initialize Class Hint
    XClassHint classhint;
    classhint.res_name = (char*) "vlc";
    classhint.res_class = (char*) "Vlc";
    XSetClassHint( XDISPLAY, m_wnd, &classhint );

    // copies WM_HINTS from the main window
    XWMHints *wm = XGetWMHints( XDISPLAY, m_rDisplay.getMainWindow() );
    if( wm )
    {
        XSetWMHints( XDISPLAY, m_wnd, wm );
        XFree( wm );
    }

    // initialize WM_CLIENT_MACHINE
    char* hostname = NULL;
    long host_name_max = sysconf( _SC_HOST_NAME_MAX );
    if( host_name_max <= 0 )
        host_name_max = _POSIX_HOST_NAME_MAX;
    hostname = new (std::nothrow) char[host_name_max];
    if( hostname && gethostname( hostname, host_name_max ) == 0 )
    {
        hostname[host_name_max - 1] = '\0';

        XTextProperty textprop;
        textprop.value = (unsigned char *) hostname;
        textprop.encoding = XA_STRING;
        textprop.format = 8;
        textprop.nitems = strlen( hostname );
        XSetWMClientMachine( XDISPLAY, m_wnd, &textprop);
    }
    delete[] hostname;

    // initialize EWMH pid
    pid_t pid = getpid();
    assert(  NET_WM_PID != None );
    XChangeProperty( XDISPLAY, m_wnd, NET_WM_PID, XA_CARDINAL, 32,
                     PropModeReplace, (unsigned char *)&pid, 1 );

    if( NET_WM_WINDOW_TYPE != None )
    {
        if( type == GenericWindow::FullscreenWindow )
        {
            // Some Window Managers like Gnome3 limit fullscreen to the
            // subarea outside the task bar if no window type is provided.
            // For those WMs, setting type window to normal ensures a clean
            // 100% fullscreen
            XChangeProperty( XDISPLAY, m_wnd, NET_WM_WINDOW_TYPE,
                         XA_ATOM, 32, PropModeReplace,
                         (unsigned char *)&NET_WM_WINDOW_TYPE_NORMAL, 1 );
        }
    }
}


X11Window::~X11Window()
{
    X11Factory *pFactory = (X11Factory*)X11Factory::instance( getIntf() );
    pFactory->m_windowMap[m_wnd] = NULL;
    pFactory->m_dndMap[m_wnd] = NULL;

    delete m_pDropTarget;

    XDestroyWindow( XDISPLAY, m_wnd );
    XSync( XDISPLAY, False );
}

void X11Window::reparent( OSWindow *win, int x, int y, int w, int h )
{
    // Reparent the window
    X11Window *parent = (X11Window*)win;
    Window new_parent =
           parent ? parent->m_wnd : DefaultRootWindow( XDISPLAY );

    XReparentWindow( XDISPLAY, m_wnd, new_parent, x, y);
    if( w && h )
        XResizeWindow( XDISPLAY, m_wnd, w, h );

    m_wnd_parent = new_parent;
}


void X11Window::show() const
{
    // Map the window
    if( m_type == GenericWindow::VoutWindow )
    {
       XLowerWindow( XDISPLAY, m_wnd );
       XMapWindow( XDISPLAY, m_wnd );
    }
    else if( m_type == GenericWindow::FullscreenWindow )
    {
        XMapRaised( XDISPLAY, m_wnd );
        setFullscreen();
        toggleOnTop( true );
    }
    else if(  m_type == GenericWindow::FscWindow )
    {
        XMapRaised( XDISPLAY, m_wnd );
        toggleOnTop( true );
    }
    else
    {
        XMapRaised( XDISPLAY, m_wnd );
    }
}


void X11Window::hide() const
{
    // Unmap the window
    XUnmapWindow( XDISPLAY, m_wnd );
}

void X11Window::moveResize( int left, int top, int width, int height ) const
{
    if( width && height )
        XMoveResizeWindow( XDISPLAY, m_wnd, left, top, width, height );
    else
        XMoveWindow( XDISPLAY, m_wnd, left, top );
}


void X11Window::raise() const
{
    XRaiseWindow( XDISPLAY, m_wnd );
}


void X11Window::setOpacity( uint8_t value ) const
{
    if( NET_WM_WINDOW_OPACITY == None )
        return;

    if( 255==value )
        XDeleteProperty(XDISPLAY, m_wnd, NET_WM_WINDOW_OPACITY);
    else
    {
        uint32_t opacity = value * ((uint32_t)-1/255);
        XChangeProperty(XDISPLAY, m_wnd, NET_WM_WINDOW_OPACITY, XA_CARDINAL, 32,
                        PropModeReplace, (unsigned char *) &opacity, 1L);
    }
    XSync( XDISPLAY, False );
}


void X11Window::setFullscreen( ) const
{
    if( NET_WM_STATE_FULLSCREEN != None )
    {
        XClientMessageEvent event;
        memset( &event, 0, sizeof( XClientMessageEvent ) );

        event.type = ClientMessage;
        event.message_type = NET_WM_STATE;
        event.display = XDISPLAY;
        event.window = m_wnd;
        event.format = 32;
        event.data.l[ 0 ] = 1;
        event.data.l[ 1 ] = NET_WM_STATE_FULLSCREEN;
 
        XSendEvent( XDISPLAY,
                    DefaultRootWindow( XDISPLAY ),
                    False, SubstructureNotifyMask|SubstructureRedirectMask,
                    (XEvent*)&event );
    }
}


void X11Window::toggleOnTop( bool onTop ) const
{
    if( NET_WM_STAYS_ON_TOP != None )
    {
        /* Switch "on top" status */
        XClientMessageEvent event;
        memset( &event, 0, sizeof( XClientMessageEvent ) );

        event.type = ClientMessage;
        event.message_type = NET_WM_STATE;
        event.display = XDISPLAY;
        event.window = m_wnd;
        event.format = 32;
        event.data.l[ 0 ] = onTop; /* set property */
        event.data.l[ 1 ] = NET_WM_STAYS_ON_TOP;

        XSendEvent( XDISPLAY, DefaultRootWindow( XDISPLAY ),
                    False, SubstructureNotifyMask|SubstructureRedirectMask, (XEvent*)&event );
    }
    else if( NET_WM_STATE_ABOVE != None )
    {
        /* Switch "above" state */
        XClientMessageEvent event;
        memset( &event, 0, sizeof( XClientMessageEvent ) );

        event.type = ClientMessage;
        event.message_type = NET_WM_STATE;
        event.display = XDISPLAY;
        event.window = m_wnd;
        event.format = 32;
        event.data.l[ 0 ] = onTop; /* set property */
        event.data.l[ 1 ] = NET_WM_STATE_ABOVE;

        XSendEvent( XDISPLAY, DefaultRootWindow( XDISPLAY ),
                    False, SubstructureNotifyMask|SubstructureRedirectMask, (XEvent*)&event );
    }
}


bool X11Window::invalidateRect( int x, int y, int w, int h ) const
{
    XClearArea( XDISPLAY, m_wnd, x, y, w, h, True );
    return true;
}

#endif
