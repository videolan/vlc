/*****************************************************************************
 * x11_api.cpp: Various x11-specific functions
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: x11_api.cpp,v 1.9 2003/06/09 22:02:13 asmax Exp $
 *
 * Authors: Cyril Deguet  <asmax@videolan.org>
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

//--- X11 -------------------------------------------------------------------
#include <X11/Xlib.h>

//--- SKIN ------------------------------------------------------------------
#include <vlc/intf.h>
#include "../src/skin_common.h"
#include "../src/theme.h"
#include "../src/window.h"
#include "../os_theme.h"
#include "../os_window.h"
#include "../os_api.h"
#include "../src/event.h"       // for MAX_PARAM_SIZE

#include <unistd.h>                                       // unkink and rmdir
#include <sys/types.h>
#include <sys/stat.h>                                               // stat()
#include <sys/time.h>                                       // gettimeofday()
#include <dirent.h>                                  // opendir() and friends

extern intf_thread_t *g_pIntf;  // ugly, but it's not my fault ;)

//---------------------------------------------------------------------------
// Event API
//---------------------------------------------------------------------------
void OSAPI_SendMessage( SkinWindow *win, unsigned int message,
                        unsigned int param1, long param2 )
{
}
//---------------------------------------------------------------------------
void OSAPI_PostMessage( SkinWindow *win, unsigned int message,
			unsigned int param1, long param2 )
{
    XEvent event;

    event.type = ClientMessage;
    event.xclient.display = g_pIntf->p_sys->display;
    event.xclient.send_event = 0;
    event.xclient.message_type = 0;
    event.xclient.format = 32;
    event.xclient.data.l[0] = message;
    event.xclient.data.l[1] = param1;
    event.xclient.data.l[2] = param2;
    
    if( win == NULL )
    {
        // broadcast message
        event.xclient.window = g_pIntf->p_sys->mainWin;
    }
    else
    {
        event.xclient.window = (( X11Window *)win)->GetHandle();
    }
    XLOCK;
    XSendEvent( g_pIntf->p_sys->display, event.xclient.window, False, 0, 
                &event );
    XUNLOCK;
}
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
// Graphic API
//---------------------------------------------------------------------------
int OSAPI_GetNonTransparentColor( int c )
{
    return ( c < 10 ? 10 : c );
}
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
// General
//---------------------------------------------------------------------------
int OSAPI_GetTime()
{
    struct timeval time;
    gettimeofday( &time, NULL );
    return( (time.tv_sec&0xffffff) * 1000 + time.tv_usec / 1000 );
}
//---------------------------------------------------------------------------
void OSAPI_GetScreenSize( int &w, int &h )
{
    Display *display = g_pIntf->p_sys->display;
    int screen = DefaultScreen( display );
    w = DisplayWidth( display, screen );
    h = DisplayHeight( display, screen );
}
//---------------------------------------------------------------------------
void OSAPI_GetMousePos( int &x, int &y )
{
    Window rootReturn, childReturn;
    int rootx, rooty;
    int winx, winy;
    unsigned int xmask;
    
    Window root = DefaultRootWindow( g_pIntf->p_sys->display );
    XLOCK;
    XQueryPointer( g_pIntf->p_sys->display, root, &rootReturn, &childReturn, 
                   &rootx, &rooty, &winx, &winy, &xmask );
    XUNLOCK;
    x = rootx;
    y = rooty;
}
//---------------------------------------------------------------------------
string OSAPI_GetWindowTitle( SkinWindow *win )
{
    return ( (X11Window *)win )->GetName();
}
//---------------------------------------------------------------------------
bool OSAPI_RmDir( string path )
{
    struct dirent *file;
    DIR *dir;

    dir = opendir( path.c_str() );
    if( !dir ) return false;

    /* Parse the directory and remove everything it contains. */
    while( (file = readdir( dir )) )
    {
        struct stat statbuf;
        string filename;

        /* Skip "." and ".." */
        if( !*file->d_name || *file->d_name == '.' ||
	    (!*(file->d_name+1) && *file->d_name == '.' &&
	     *(file->d_name+1) == '.') )
        {
            continue;
        }

        filename += path + "/";
        filename += file->d_name;

        if( !stat( filename.c_str(), &statbuf ) && statbuf.st_mode & S_IFDIR )
        {
            OSAPI_RmDir( filename );
        }
        else
        {
            unlink( filename.c_str() );
        }
    }

    /* Close the directory */
    closedir( dir );

    rmdir( path.c_str() );

    return true;
}
//---------------------------------------------------------------------------

#endif
