/*****************************************************************************
 * gtk2_event.cpp: GTK2 implementation of the Event class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: gtk2_event.cpp,v 1.6 2003/04/15 20:33:58 karibu Exp $
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

#if !defined WIN32

//--- GTK2 -----------------------------------------------------------------
#include <gdk/gdk.h>

//--- VLC -------------------------------------------------------------------
#include <vlc/intf.h>

//--- SKIN ------------------------------------------------------------------
#include "os_api.h"
#include "event.h"
#include "os_event.h"
#include "window.h"
#include "os_window.h"
#include "theme.h"
#include "os_theme.h"
#include "skin_common.h"


//---------------------------------------------------------------------------
//   VLC Event
//---------------------------------------------------------------------------
GTK2Event::GTK2Event( intf_thread_t *p_intf, string Desc, string shortcut )
    : Event( p_intf, Desc, shortcut )
{
    gWnd = NULL;
}
//---------------------------------------------------------------------------
GTK2Event::GTK2Event( intf_thread_t *p_intf, GdkWindow *gwnd, unsigned int msg,
    unsigned int par1, long par2 ) : Event( p_intf, msg, par1, par2 )
{
    gWnd = gwnd;
}
//---------------------------------------------------------------------------
GTK2Event::GTK2Event( intf_thread_t *p_intf, Window *win, unsigned int msg,
    unsigned int par1, long par2 ) : Event( p_intf, msg, par1, par2 )
{
    gWnd = ( (GTK2Window *)win )->GetHandle();
}
//---------------------------------------------------------------------------
GTK2Event::~GTK2Event()
{
}
//---------------------------------------------------------------------------
bool GTK2Event::SendEvent()
{
    if( Message != VLC_NOTHING )
    {
        // Find window matching with gwnd
        list<Window *>::const_iterator win;
        for( win = p_intf->p_sys->p_theme->WindowList.begin();
             win != p_intf->p_sys->p_theme->WindowList.end(); win++ )
        {
            // If it is the correct window
            if( gWnd == ( (GTK2Window *)(*win) )->GetHandle() )
            {
                OSAPI_PostMessage( *win, Message, Param1, Param2 );
                PostSynchroMessage();
            }
        }
    }

    OSAPI_PostMessage( NULL, Message, Param1, Param2 );
    return true;
}
//---------------------------------------------------------------------------
bool GTK2Event::IsEqual( Event *evt )
{
/*    GTK2Event *WinEvt = (GTK2Event *)evt;
    return( WinEvt->GetWindow() == hWnd   && WinEvt->GetMessage() == Message &&
            WinEvt->GetParam1() == Param1 && WinEvt->GetParam2()  == Param2 );*/
}
//---------------------------------------------------------------------------
void GTK2Event::CreateOSEvent( string para1, string para2, string para3 )
{
    // Find Parameters
    switch( Message )
    {
        case WINDOW_MOVE:
            gWnd = GetWindowFromName( para1 );
            break;

        case WINDOW_CLOSE:
            gWnd = GetWindowFromName( para1 );
            break;

        case WINDOW_OPEN:
            gWnd = GetWindowFromName( para1 );
            break;

    }
}
//---------------------------------------------------------------------------
GdkWindow *GTK2Event::GetWindowFromName( string name )
{
    GTK2Window *win = (GTK2Window *)
        p_intf->p_sys->p_theme->GetWindow( name );

    if( win == NULL )
    {
        return NULL;
    }
    else
    {
        return win->GetHandle();
    }
}
//---------------------------------------------------------------------------

#endif
