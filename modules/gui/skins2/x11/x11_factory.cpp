/*****************************************************************************
 * x11_factory.cpp
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

#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xinerama.h>

#include "x11_factory.hpp"
#include "x11_display.hpp"
#include "x11_graphics.hpp"
#include "x11_loop.hpp"
#include "x11_popup.hpp"
#include "x11_timer.hpp"
#include "x11_window.hpp"
#include "x11_tooltip.hpp"

#include "../src/generic_window.hpp"

#include <vlc_common.h>
#include <vlc_xlib.h>

X11Factory::X11Factory( intf_thread_t *pIntf ): OSFactory( pIntf ),
    m_pDisplay( NULL ), m_pTimerLoop( NULL ), m_dirSep( "/" ),
    mPointerWindow( None ), mVoutWindow( None ), mEmptyCursor( None )
{
    // see init()
}


X11Factory::~X11Factory()
{
    if( mEmptyCursor != None )
        XFreeCursor( m_pDisplay->getDisplay(), mEmptyCursor );
    delete m_pTimerLoop;
    delete m_pDisplay;
}


bool X11Factory::init()
{
    // make sure xlib is safe-thread
    if( !vlc_xlib_init( VLC_OBJECT( getIntf() ) ) )
    {
        msg_Err( getIntf(), "initializing xlib for multi-threading failed" );
        return false;
    }

    // Create the X11 display
    m_pDisplay = new X11Display( getIntf() );

    // Get the display
    Display *pDisplay = m_pDisplay->getDisplay();
    if( pDisplay == NULL )
    {
        // Initialization failed
        return false;
    }

    // Create the timer loop
    m_pTimerLoop = new X11TimerLoop( getIntf(),
                                     ConnectionNumber( pDisplay ) );

    // Initialize the resource path
    char *datadir = config_GetUserDir( VLC_USERDATA_DIR );
    m_resourcePath.push_back( (std::string)datadir + "/skins2" );
    free( datadir );
    m_resourcePath.push_back( (std::string)"share/skins2" );
    datadir = config_GetSysPath(VLC_PKG_DATA_DIR, "skins2");
    m_resourcePath.push_back( (std::string)datadir );
    free( datadir );

    // Determine the monitor geometry
    getDefaultGeometry( &m_screenWidth, &m_screenHeight );

    // list all available monitors
    int num_screen;
    XineramaScreenInfo* info = XineramaQueryScreens( pDisplay, &num_screen );
    if( info )
    {
        msg_Dbg( getIntf(), "number of monitors detected : %i", num_screen );
        for( int i = 0; i < num_screen; i++ )
            msg_Dbg( getIntf(), "  monitor #%i : %ix%i at +%i+%i",
                                i, info[i].width, info[i].height,
                                info[i].x_org, info[i].y_org );
        XFree( info );
    }

    // init cursors
    initCursors();

    return true;
}


OSGraphics *X11Factory::createOSGraphics( int width, int height )
{
    return new X11Graphics( getIntf(), *m_pDisplay, width, height );
}


OSLoop *X11Factory::getOSLoop()
{
    return X11Loop::instance( getIntf(), *m_pDisplay );
}


void X11Factory::destroyOSLoop()
{
    X11Loop::destroy( getIntf() );
}

void X11Factory::minimize()
{
    XIconifyWindow( m_pDisplay->getDisplay(), m_pDisplay->getMainWindow(),
                    DefaultScreen( m_pDisplay->getDisplay() ) );
}

void X11Factory::restore()
{
    // TODO
}

void X11Factory::addInTray()
{
    // TODO
}

void X11Factory::removeFromTray()
{
    // TODO
}

void X11Factory::addInTaskBar()
{
    // TODO
}

void X11Factory::removeFromTaskBar()
{
    // TODO
}

OSTimer *X11Factory::createOSTimer( CmdGeneric &rCmd )
{
    return new X11Timer( getIntf(), rCmd );
}


OSWindow *X11Factory::createOSWindow( GenericWindow &rWindow, bool dragDrop,
                                      bool playOnDrop, OSWindow *pParent,
                                      GenericWindow::WindowType_t type )
{
    return new X11Window( getIntf(), rWindow, *m_pDisplay, dragDrop,
                          playOnDrop, (X11Window*)pParent, type );
}


OSTooltip *X11Factory::createOSTooltip()
{
    return new X11Tooltip( getIntf(), *m_pDisplay );
}


OSPopup *X11Factory::createOSPopup()
{
    return new X11Popup( getIntf(), *m_pDisplay );
}


int X11Factory::getScreenWidth() const
{
    return m_screenWidth;
}


int X11Factory::getScreenHeight() const
{
    return m_screenHeight;
}


void X11Factory::getMonitorInfo( OSWindow *pWindow,
                                 int* p_x, int* p_y,
                                 int* p_width, int* p_height ) const
{
    X11Window *pWin = (X11Window*)pWindow;
    // initialize to default geometry
    *p_x = 0;
    *p_y = 0;
    *p_width = getScreenWidth();
    *p_height = getScreenHeight();

    // Use Xinerama to determine the monitor where the video
    // mostly resides (biggest surface)
    Display *pDisplay = m_pDisplay->getDisplay();
    Window wnd = pWin->getDrawable();
    Window root = DefaultRootWindow( pDisplay );
    Window child_wnd;

    int x, y;
    unsigned int w, h, border, depth;
    XGetGeometry( pDisplay, wnd, &root, &x, &y, &w, &h, &border, &depth );
    XTranslateCoordinates( pDisplay, wnd, root, 0, 0, &x, &y, &child_wnd );

    int num;
    XineramaScreenInfo* info = XineramaQueryScreens( pDisplay, &num );
    if( info )
    {
        Region reg1 = XCreateRegion();
        XRectangle rect1 = { (short)x, (short)y, (unsigned short)w, (unsigned short)h };
        XUnionRectWithRegion( &rect1, reg1, reg1 );

        unsigned int surface = 0;
        for( int i = 0; i < num; i++ )
        {
            Region reg2 = XCreateRegion();
            XRectangle rect2 = { info[i].x_org, info[i].y_org,
                                 (unsigned short)info[i].width, (unsigned short)info[i].height };
            XUnionRectWithRegion( &rect2, reg2, reg2 );

            Region reg = XCreateRegion();
            XIntersectRegion( reg1, reg2, reg );
            XRectangle rect;
            XClipBox( reg, &rect );
            unsigned int surf = rect.width * rect.height;
            if( surf > surface )
            {
               surface = surf;
               *p_x = info[i].x_org;
               *p_y = info[i].y_org;
               *p_width = info[i].width;
               *p_height = info[i].height;
            }
            XDestroyRegion( reg );
            XDestroyRegion( reg2 );
        }
        XDestroyRegion( reg1 );
        XFree( info );
    }
}


void X11Factory::getMonitorInfo( int numScreen,
                                 int* p_x, int* p_y,
                                 int* p_width, int* p_height ) const
{
    // initialize to default geometry
    *p_x = 0;
    *p_y = 0;
    *p_width = getScreenWidth();
    *p_height = getScreenHeight();

    // try to detect the requested screen via Xinerama
    if( numScreen >= 0 )
    {
        int num;
        Display *pDisplay = m_pDisplay->getDisplay();
        XineramaScreenInfo* info = XineramaQueryScreens( pDisplay, &num );
        if( info )
        {
            if( numScreen < num )
            {
                *p_x = info[numScreen].x_org;
                *p_y = info[numScreen].y_org;
                *p_width = info[numScreen].width;
                *p_height = info[numScreen].height;
            }
            XFree( info );
        }
    }
}


void X11Factory::getDefaultGeometry( int* p_width, int* p_height ) const
{
    Display *pDisplay = m_pDisplay->getDisplay();

    // Initialize to defaults
    int screen = DefaultScreen( pDisplay );
    *p_width = DisplayWidth( pDisplay, screen );
    *p_height = DisplayHeight( pDisplay, screen );

    // Use Xinerama to restrain to the first monitor instead of the full
    // virtual screen
    int num;
    XineramaScreenInfo* info = XineramaQueryScreens( pDisplay, &num );
    if( info )
    {
        for( int i = 0; i < num; i++ )
        {
            if( info[i].x_org == 0 && info[i].y_org == 0 )
            {
                *p_width = info[i].width;
                *p_height = info[i].height;
                break;
            }
        }
        XFree( info );
    }
}


SkinsRect X11Factory::getWorkArea() const
{
    // query Work Area if available from Window Manager
    // otherwise, default to the whole screen
    int x = 0, y = 0;
    int w = getScreenWidth(), h = getScreenHeight();
    if( m_pDisplay->m_net_workarea != None )
    {
        Atom ret;
        int i_format;
        unsigned long i_items, i_bytesafter;
        long *values;
        if( XGetWindowProperty( m_pDisplay->getDisplay(),
                                DefaultRootWindow( m_pDisplay->getDisplay() ),
                                m_pDisplay->m_net_workarea,
                                0, 16384, False, XA_CARDINAL,
                                &ret, &i_format, &i_items, &i_bytesafter,
                                (unsigned char **)&values ) == Success )
        {
            if( values )
            {
                x = values[0];
                y = values[1];
                w = values[2];
                h = values[3];
                XFree( values );
            }
        }
    }
    msg_Dbg( getIntf(),"WorkArea: %ix%i at +%i+%i", w, h, x, y );
    return SkinsRect( x, y, x + w, y + h );
}


void X11Factory::getMousePos( int &rXPos, int &rYPos ) const
{
    Window rootReturn, childReturn;
    int winx, winy;
    unsigned int xmask;

    Display *pDisplay = m_pDisplay->getDisplay();
    Window root = DefaultRootWindow( pDisplay );
    XQueryPointer( pDisplay, root, &rootReturn, &childReturn,
                   &rXPos, &rYPos, &winx, &winy, &xmask );
}


void X11Factory::rmDir( const std::string &rPath )
{
    struct dirent *file;
    DIR *dir;

    dir = opendir( rPath.c_str() );
    if( !dir ) return;

    // Parse the directory and remove everything it contains
    while( (file = readdir( dir )) )
    {
        std::string filename = file->d_name;

        // Skip "." and ".."
        if( filename == "." || filename == ".." )
        {
            continue;
        }

        filename = rPath + "/" + filename;

        if( rmdir( filename.c_str() ) && errno == ENOTDIR )
            unlink( filename.c_str() );
    }

    // Close the directory
    closedir( dir );

    // And delete it
    rmdir( rPath.c_str() );
}


void X11Factory::changeCursor( CursorType_t type ) const
{
    Cursor cursor = mCursors[type];
    Window win = (type == OSFactory::kNoCursor) ? mVoutWindow : mPointerWindow;

    if( win != None )
        XDefineCursor( m_pDisplay->getDisplay(), win, cursor );
}

void X11Factory::initCursors( )
{
    Display *display = m_pDisplay->getDisplay();
    static const struct {
        CursorType_t type;
        const char *name;
    } cursors[] = {
        { kDefaultArrow, "left_ptr" },
        { kResizeNWSE, "bottom_right_corner" },
        { kResizeNS, "bottom_side" },
        { kResizeWE, "right_side" },
        { kResizeNESW, "bottom_left_corner" },
    };
    // retrieve cursors from default theme
    for( unsigned i = 0; i < sizeof(cursors) / sizeof(cursors[0]); i++ )
        mCursors[cursors[i].type] =
            XcursorLibraryLoadCursor( display, cursors[i].name );

    // build an additional empty cursor
    XColor color;
    const char data[] = { 0 };
    Window root = DefaultRootWindow( display );
    Pixmap pix = XCreateBitmapFromData( display, root, data, 1, 1 );
    mEmptyCursor = XCreatePixmapCursor( display, pix, pix, &color, &color, 0, 0 );
    XFreePixmap( display, pix );
    mCursors[kNoCursor] = mEmptyCursor;
}


void X11Factory::setPointerWindow( Window win )
{
    GenericWindow *pWin = m_windowMap[win];
    if( pWin->getType() == GenericWindow::VoutWindow )
        mVoutWindow = win;
    mPointerWindow = win;
}
#endif
