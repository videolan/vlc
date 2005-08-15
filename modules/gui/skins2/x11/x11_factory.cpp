/*****************************************************************************
 * x11_factory.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifdef X11_SKINS

#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <X11/Xlib.h>

#include "x11_factory.hpp"
#include "x11_display.hpp"
#include "x11_graphics.hpp"
#include "x11_loop.hpp"
#include "x11_timer.hpp"
#include "x11_window.hpp"
#include "x11_tooltip.hpp"


X11Factory::X11Factory( intf_thread_t *pIntf ): OSFactory( pIntf ),
    m_pDisplay( NULL ), m_pTimerLoop( NULL ), m_dirSep( "/" )
{
    // see init()
}


X11Factory::~X11Factory()
{
    delete m_pTimerLoop;
    delete m_pDisplay;
}


bool X11Factory::init()
{
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
    m_resourcePath.push_back( (string)getIntf()->p_vlc->psz_homedir +
        m_dirSep + CONFIG_DIR + "/skins2" );
    m_resourcePath.push_back( (string)"share/skins2" );
    m_resourcePath.push_back( (string)DATA_PATH + "/skins2" );

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

OSTimer *X11Factory::createOSTimer( CmdGeneric &rCmd )
{
    return new X11Timer( getIntf(), rCmd );
}


OSWindow *X11Factory::createOSWindow( GenericWindow &rWindow, bool dragDrop,
                                      bool playOnDrop, OSWindow *pParent )
{
    return new X11Window( getIntf(), rWindow, *m_pDisplay, dragDrop,
                          playOnDrop, (X11Window*)pParent );
}


OSTooltip *X11Factory::createOSTooltip()
{
    return new X11Tooltip( getIntf(), *m_pDisplay );
}


int X11Factory::getScreenWidth() const
{
    Display *pDisplay = m_pDisplay->getDisplay();
    int screen = DefaultScreen( pDisplay );
    return DisplayWidth( pDisplay, screen );
}


int X11Factory::getScreenHeight() const
{
    Display *pDisplay = m_pDisplay->getDisplay();
    int screen = DefaultScreen( pDisplay );
    return DisplayHeight( pDisplay, screen );
}


Rect X11Factory::getWorkArea() const
{
    // XXX
    return Rect( 0, 0, getScreenWidth(), getScreenHeight() );
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


void X11Factory::rmDir( const string &rPath )
{
    struct dirent *file;
    DIR *dir;

    dir = opendir( rPath.c_str() );
    if( !dir ) return;

    // Parse the directory and remove everything it contains
    while( (file = readdir( dir )) )
    {
        struct stat statbuf;
        string filename = file->d_name;

        // Skip "." and ".."
        if( filename == "." || filename == ".." )
        {
            continue;
        }

        filename = rPath + "/" + filename;

        if( !stat( filename.c_str(), &statbuf ) && statbuf.st_mode & S_IFDIR )
        {
            rmDir( filename );
        }
        else
        {
            unlink( filename.c_str() );
        }
    }

    // Close the directory
    closedir( dir );

    // And delete it
    rmdir( rPath.c_str() );
}


#endif
