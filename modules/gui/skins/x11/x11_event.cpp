/*****************************************************************************
 * x11_event.cpp: x11 implementation of the Event class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: x11_event.cpp,v 1.5 2003/06/22 12:54:03 asmax Exp $
 *
 * Authors: Cyril Deguet     <asmax@videolan.org>
 *          Emmanuel Puig    <karibu@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111,
 * USA.
 *****************************************************************************/

#ifdef X11_SKINS

//--- X11 -------------------------------------------------------------------
#include <X11/Xlib.h>

//--- VLC -------------------------------------------------------------------
#include <vlc/intf.h>

//--- SKIN ------------------------------------------------------------------
#include "../os_api.h"
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
X11Event::X11Event( intf_thread_t *p_intf, string Desc, string shortcut )
    : Event( p_intf, Desc, shortcut )
{
    Wnd = None;
}
//---------------------------------------------------------------------------
X11Event::X11Event( intf_thread_t *p_intf, Window wnd, unsigned int msg,
    unsigned int par1, long par2 ) : Event( p_intf, msg, par1, par2 )
{
    Wnd = wnd;
}
//---------------------------------------------------------------------------
X11Event::X11Event( intf_thread_t *p_intf, SkinWindow *win, unsigned int msg,
    unsigned int par1, long par2 ) : Event( p_intf, msg, par1, par2 )
{
    Wnd = ( (X11Window *)win )->GetHandle();
}
//---------------------------------------------------------------------------
X11Event::~X11Event()
{
}
//---------------------------------------------------------------------------
bool X11Event::SendEvent()
{
    if( Message != VLC_NOTHING )
    {
        // Find window matching with Wnd
        list<SkinWindow *>::const_iterator win;
        for( win = p_intf->p_sys->p_theme->WindowList.begin();
             win != p_intf->p_sys->p_theme->WindowList.end(); win++ )
        {
            // If it is the correct window
            if( Wnd == ( (X11Window *)(*win) )->GetHandle() )
            {
                OSAPI_PostMessage( *win, Message, Param1, Param2 );
                PostSynchroMessage();
                return true;
            }
        }
        OSAPI_PostMessage( NULL, Message, Param1, Param2 );
        return true;
    }

    return false;
}
//---------------------------------------------------------------------------
bool X11Event::IsEqual( Event *evt )
{
    X11Event *XEvt = (X11Event *)evt;
    return( XEvt->GetWindow() == Wnd   && XEvt->GetMessage() == Message &&
            XEvt->GetParam1() == Param1 && XEvt->GetParam2()  == Param2 );
}
//---------------------------------------------------------------------------
void X11Event::CreateOSEvent( string para1, string para2, string para3 )
{
    // Find Parameters
    switch( Message )
    {
        case WINDOW_MOVE:
            Wnd = GetWindowFromName( para1 );
            break;

        case WINDOW_CLOSE:
            Wnd = GetWindowFromName( para1 );
            break;

        case WINDOW_OPEN:
            Wnd = GetWindowFromName( para1 );
            break;

    }
}
//---------------------------------------------------------------------------
Window X11Event::GetWindowFromName( string name )
{
    X11Window *win = (X11Window *)
        p_intf->p_sys->p_theme->GetWindow( name );

    if( win == NULL )
    {
        return None;
    }
    else
    {
        return win->GetHandle();
    }
}
//---------------------------------------------------------------------------

#endif
