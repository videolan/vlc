/*****************************************************************************
 * gtk2_api.cpp: Various gtk2-specific functions
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: gtk2_api.cpp,v 1.11 2003/04/16 21:40:07 ipkiss Exp $
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

#if !defined WIN32

//--- GTK2 ------------------------------------------------------------------
#include <glib.h>
#include <gdk/gdk.h>

//--- SKIN ------------------------------------------------------------------
#include "../src/window.h"
#include "../os_window.h"
#include "../os_api.h"
#include "../src/event.h"       // for MAX_PARAM_SIZE

#include <stdio.h>

//---------------------------------------------------------------------------
// Event API
//---------------------------------------------------------------------------
void OSAPI_SendMessage( Window *win, unsigned int message, unsigned int param1,
                        long param2 )
{
/*    if( win == NULL )
        SendMessage( NULL, message, param1, param2 );
    else
        SendMessage( ( (Win32Window *)win )->GetHandle(), message, param1,
                     param2 );*/
}
//---------------------------------------------------------------------------
void OSAPI_PostMessage( Window *win, unsigned int message, unsigned int param1,
                        long param2 )
{
    GdkEventClient *event = new GdkEventClient;
    event->type = GDK_CLIENT_EVENT;
    if( win == NULL )
        event->window = NULL;
    else
        event->window = ((GTK2Window *)win)->GetHandle();
    event->send_event = 0;
    event->message_type = NULL;
    event->data_format = 32;
    event->data.l[0] = message;
    event->data.l[1] = param1;
    event->data.l[2] = param2;

    gdk_event_put( (GdkEvent *)event );

    delete event;
}
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
// Graphic API
//---------------------------------------------------------------------------
int OSAPI_GetNonTransparentColor( int c )
{
/*    // Get desktop device context
    HDC DeskDC = GetWindowDC( GetDesktopWindow() );

    // If color is black or color is same as black wether pixel color depth
    if( c == 0 || SetPixel( DeskDC, 0, 0, c ) == 0 )
    {
        if( GetDeviceCaps( DeskDC, BITSPIXEL ) < 24 )
            c = RGB(8, 0, 0);
        else
            c = RGB(1, 0, 0);
    }
    ReleaseDC( GetDesktopWindow(), DeskDC );
    return c;*/
}
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
// General
//---------------------------------------------------------------------------
int OSAPI_GetTime()
{
    GTimeVal time;
    g_get_current_time( &time );
    return ( time.tv_sec * 1000 + time.tv_usec / 1000 );
}
//---------------------------------------------------------------------------
void OSAPI_GetScreenSize( int &w, int &h )
{
/*    w = GetSystemMetrics(SM_CXSCREEN);
    h = GetSystemMetrics(SM_CYSCREEN);*/
}
//---------------------------------------------------------------------------
void OSAPI_GetMousePos( int &x, int &y )
{
    gdk_window_get_pointer( gdk_get_default_root_window(), &x, &y, NULL );
}
//---------------------------------------------------------------------------
string OSAPI_GetWindowTitle( Window *win )
{
    return ( (GTK2Window *)win )->GetName();
}
//---------------------------------------------------------------------------
bool OSAPI_RmDir( string path )
{
/*    WIN32_FIND_DATA find;
    string File;
    string FindFiles = path + "\\*.*";
    HANDLE handle    = FindFirstFile( (char *)FindFiles.c_str(), &find );

    while( handle != INVALID_HANDLE_VALUE )
    {
        // If file is neither "." nor ".."
        if( strcmp( find.cFileName, "." ) && strcmp( find.cFileName, ".." ) )
        {
            // Set file name
            File = path + "\\" + (string)find.cFileName;

            // If file is a directory, delete it recursively
            if( find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
            {
                OSAPI_RmDir( File );
            }
            // Else, it is a file so simply delete it
            else
            {
                DeleteFile( (char *)File.c_str() );
            }
        }

        // If no more file in directory, exit while
        if( !FindNextFile( handle, &find ) )
            break;
    }

    // Now directory is empty so can be removed
    FindClose( handle );
    RemoveDirectory( (char *)path.c_str() );

    return true;*/
}
//---------------------------------------------------------------------------

#endif
