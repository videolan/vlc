/*****************************************************************************
 * gtk2_run.cpp:
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: gtk2_run.cpp,v 1.9 2003/04/16 19:22:53 karibu Exp $
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

//--- GTK2 ------------------------------------------------------------------
#include <glib.h>
#include <gdk/gdk.h>

//--- VLC -------------------------------------------------------------------
#include <vlc/intf.h>

//--- SKIN ------------------------------------------------------------------
#include "os_api.h"
#include "event.h"
#include "os_event.h"
#include "banks.h"
#include "window.h"
#include "os_window.h"
#include "theme.h"
#include "os_theme.h"
#include "skin_common.h"
#include "vlcproc.h"


//---------------------------------------------------------------------------
// Specific method
//---------------------------------------------------------------------------
bool IsVLCEvent( unsigned int msg );
int  SkinManage( intf_thread_t *p_intf );



//---------------------------------------------------------------------------
// REFRESH TIMER CALLBACK
//---------------------------------------------------------------------------
/*void CALLBACK RefreshTimer( HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime )
{
    intf_thread_t *p_intf = (intf_thread_t *)GetWindowLongPtr( hwnd,
        GWLP_USERDATA );
    SkinManage( p_intf );
}*/
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// GTK2 interface
//---------------------------------------------------------------------------
void GTK2Proc( GdkEvent *event, gpointer data )
{
    // Get pointer to thread info
    unsigned int msg;
    VlcProc *proc = (VlcProc *)data;
    intf_thread_t *p_intf = proc->GetpIntf();
    Event *evt;
    list<Window *>::const_iterator win;
    GdkWindow *gwnd = ((GdkEventAny *)event)->window;

    // Create event to dispatch in windows
    // Skin event
    if( event->type == GDK_CLIENT_EVENT )
    {
        msg = ( (GdkEventClient *)event )->data.l[0];
        evt = (Event *)new OSEvent( p_intf, 
            ((GdkEventAny *)event)->window,
            msg,
            ( (GdkEventClient *)event )->data.l[1],
            ( (GdkEventClient *)event )->data.l[2] );
    }
    // System event
    else
    {
        msg = event->type;
        evt = (Event *)new OSEvent( p_intf, 
            ((GdkEventAny *)event)->window, msg, 0, (long)event );
    }

    // Send event
    if( IsVLCEvent( msg ) )
    {
        if( !proc->EventProc( evt ) )
        {
            fprintf( stderr, "Quit\n" );
            g_main_context_unref( g_main_context_default() );
            return;      // Exit VLC !
        }
    }
    else if( gwnd == NULL )
    {
        for( win = p_intf->p_sys->p_theme->WindowList.begin();
             win != p_intf->p_sys->p_theme->WindowList.end(); win++ )
        {
            (*win)->ProcessEvent( evt );
        }
    }
    else
    {
        // Find window matching with gwnd
        for( win = p_intf->p_sys->p_theme->WindowList.begin();
             win != p_intf->p_sys->p_theme->WindowList.end(); win++ )
        {
            // If it is the correct window
            if( gwnd == ( (GTK2Window *)(*win) )->GetHandle() )
            {
                // Send event and check if processed
                if( (*win)->ProcessEvent( evt ) )
                {
                    delete (OSEvent *)evt;
                    return;
                }
                else
                {
                    break;
                }
            }
        }
    }

    evt->DestructParameters();
    delete (OSEvent *)evt;

    // Check if vlc is closing
    proc->IsClosing();

#if 0
    // If Window is parent window
    if( hwnd == ( (GTK2Theme *)p_intf->p_sys->p_theme )->GetParentWindow() )
    {
        if( uMsg == WM_SYSCOMMAND )
        {
            if( (Event *)wParam != NULL )
                ( (Event *)wParam )->SendEvent();
            return 0;
        }
        else if( uMsg == WM_RBUTTONDOWN && wParam == 42 &&
                 lParam == WM_RBUTTONDOWN )
        {
            int x, y;
            OSAPI_GetMousePos( x, y );
            TrackPopupMenu(
                ( (GTK2Theme *)p_intf->p_sys->p_theme )->GetSysMenu(),
                0, x, y, 0, hwnd, NULL );
        }
    }


    // If closing parent window
    if( uMsg == WM_CLOSE )
    {
        OSAPI_PostMessage( NULL, VLC_HIDE, VLC_QUIT, 0 );
        return 0;
    }

    // If hwnd does not match any window or message not processed
    return DefWindowProc( hwnd, uMsg, wParam, lParam );
#endif
}
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
// GTK2 interface
//---------------------------------------------------------------------------
void OSRun( intf_thread_t *p_intf )
{
    // Create VLC event object processing
    VlcProc *proc = new VlcProc( p_intf );

    gdk_event_handler_set( GTK2Proc, (gpointer)proc, NULL );

    // Main event loop
    GMainLoop *loop = g_main_loop_new( NULL, TRUE );
    g_main_loop_run( loop );
}
//---------------------------------------------------------------------------
bool IsVLCEvent( unsigned int msg )
{
    return( msg > VLC_MESSAGE && msg < VLC_WINDOW );
}
//---------------------------------------------------------------------------

#endif
