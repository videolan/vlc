/*****************************************************************************
 * win32_event.h: Win32 implementation of the Event class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: win32_event.h,v 1.3 2003/04/21 21:51:16 asmax Exp $
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

#ifdef WIN32

#ifndef VLC_SKIN_WIN32_EVENT
#define VLC_SKIN_WIN32_EVENT

//--- GENERAL ---------------------------------------------------------------
#include <string>
using namespace std;

//---------------------------------------------------------------------------
struct intf_thread_t;
class SkinWindow;

//---------------------------------------------------------------------------
class Win32Event : Event
{
    private:
        HWND GetWindowFromName( string name );
        HWND hWnd;
    public:
        // Constructor
        Win32Event( intf_thread_t *p_intf, string Desc, string shortcut );
        Win32Event( intf_thread_t *p_intf, HWND hwnd, unsigned int msg,
                    unsigned int par1, long par2 );
        Win32Event( intf_thread_t *p_intf, SkinWindow *win, unsigned int msg,
                    unsigned int par1, long par2 );

        // Destructor
        virtual ~Win32Event();

        // Event sending
        virtual bool SendEvent();

        // General operations on events
        virtual void CreateOSEvent( string para1, string para2, string para3 );
        virtual bool IsEqual( Event *evt );

        // Getters
        HWND GetWindow()    { return hWnd; }
};
//---------------------------------------------------------------------------

#endif

#endif
