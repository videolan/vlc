/*****************************************************************************
 * win32_event.cpp: Win32 implementation of the Event class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: win32_event.cpp,v 1.4 2003/04/16 21:40:07 ipkiss Exp $
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

//--- WIN32 -----------------------------------------------------------------
#include <windows.h>

//--- VLC -------------------------------------------------------------------
#include <vlc/intf.h>

//--- SKIN ------------------------------------------------------------------
#include "../src/event.h"
#include "../os_event.h"
#include "../src/window.h"
#include "../os_window.h"
#include "../src/theme.h"
#include "../os_theme.h"
#include "../src/skin_common.h"



//---------------------------------------------------------------------------
//   VLC Event
//---------------------------------------------------------------------------
Win32Event::Win32Event( intf_thread_t *p_intf, string Desc, string shortcut )
    : Event( p_intf, Desc, shortcut )
{
    hWnd = NULL;
}
//---------------------------------------------------------------------------
Win32Event::Win32Event( intf_thread_t *p_intf, HWND hwnd, unsigned int msg,
    unsigned int par1, long par2 ) : Event( p_intf, msg, par1, par2 )
{
    hWnd = hwnd;
}
//---------------------------------------------------------------------------
Win32Event::Win32Event( intf_thread_t *p_intf, Window *win, unsigned int msg,
    unsigned int par1, long par2 ) : Event( p_intf, msg, par1, par2 )
{
    hWnd = ( (Win32Window *)win )->GetHandle();
}
//---------------------------------------------------------------------------
Win32Event::~Win32Event()
{
}
//---------------------------------------------------------------------------
bool Win32Event::SendEvent()
{
    if( Message != VLC_NOTHING )
    {
        PostMessage( hWnd, Message, Param1, Param2 );
        PostSynchroMessage();
        return true;
    }

    return true;

}
//---------------------------------------------------------------------------
bool Win32Event::IsEqual( Event *evt )
{
    Win32Event *WinEvt = (Win32Event *)evt;
    return( WinEvt->GetWindow() == hWnd   && WinEvt->GetMessage() == Message &&
            WinEvt->GetParam1() == Param1 && WinEvt->GetParam2()  == Param2 );
}
//---------------------------------------------------------------------------
void Win32Event::CreateOSEvent( string para1, string para2, string para3 )
{

    // Find Parameters
    switch( Message )
    {
        case WINDOW_MOVE:
            hWnd = GetWindowFromName( para1 );
            break;

        case WINDOW_CLOSE:
            hWnd = GetWindowFromName( para1 );
            break;

        case WINDOW_OPEN:
            hWnd = GetWindowFromName( para1 );
            break;

    }

}
//---------------------------------------------------------------------------
HWND Win32Event::GetWindowFromName( string name )
{
    Win32Window *win = (Win32Window *)
        p_intf->p_sys->p_theme->GetWindow( name );
    if( win == NULL )
        return NULL;
    else
        return win->GetHandle();
}
//---------------------------------------------------------------------------

#endif
