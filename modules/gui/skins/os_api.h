/*****************************************************************************
 * os_api.h: Wrapper for some os-specific functions
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: os_api.h,v 1.3 2003/04/21 21:51:16 asmax Exp $
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


#ifndef VLC_SKIN_API
#define VLC_SKIN_API

#if defined( WIN32 )
    #define DIRECTORY_SEPARATOR '\\'
#else
    #define DIRECTORY_SEPARATOR '/'
#endif


//--- GENERAL ---------------------------------------------------------------
#include <string>
using namespace std;

//---------------------------------------------------------------------------
class SkinWindow;

//---------------------------------------------------------------------------
// Event API
//---------------------------------------------------------------------------
// This functions send a message to the interface message stack
// First argument is the destination window (NULL is broadcasting)
// Second argument is the name of the message (see event.h)
// Other parameters are the arguments of message
// Send message is supposed to treat the message directly and should not be
// used if possible
void OSAPI_SendMessage( SkinWindow *win, unsigned int message, unsigned int param1,
                        long param2 );
void OSAPI_PostMessage( SkinWindow *win, unsigned int message, unsigned int param1,
                        long param2 );

//---------------------------------------------------------------------------
// Graphic API
//---------------------------------------------------------------------------
// This function get a color and return the correspounding color regarding
// color resolution. If it is black, it should return the nearest non black
// color
int  OSAPI_GetNonTransparentColor( int c );

// This function get the size in pixel of the screen
void OSAPI_GetScreenSize( int &w, int &h );

//---------------------------------------------------------------------------
// General
//---------------------------------------------------------------------------
// This function get the position in pixel of the mouse cursor position
void OSAPI_GetMousePos( int &x, int &y );

// This function returns the Title of the specified window
string OSAPI_GetWindowTitle( SkinWindow *win );

// This functions removes a directory and all its contents
bool OSAPI_RmDir( string Path );

// This function returns a time in millisecond whose reference should be fixed
int  OSAPI_GetTime();

//---------------------------------------------------------------------------

#endif
