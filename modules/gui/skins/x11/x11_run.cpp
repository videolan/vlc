/*****************************************************************************
 * x11_run.cpp:
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: x11_run.cpp,v 1.14 2003/06/03 22:18:58 gbazin Exp $
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

#ifdef X11_SKINS

//--- X11 -------------------------------------------------------------------
#include <X11/Xlib.h>

//--- VLC -------------------------------------------------------------------
#include <vlc/intf.h>

//--- SKIN ------------------------------------------------------------------
#include "../os_api.h"
#include "../src/event.h"
#include "../os_event.h"
#include "../src/banks.h"
#include "../src/window.h"
#include "../os_window.h"
#include "../src/theme.h"
#include "../os_theme.h"
#include "../src/skin_common.h"
#include "../src/vlcproc.h"

//---------------------------------------------------------------------------
// Specific method
//---------------------------------------------------------------------------
bool IsVLCEvent( unsigned int msg );
int  SkinManage( intf_thread_t *p_intf );

//---------------------------------------------------------------------------
// X11 event processing
//---------------------------------------------------------------------------
int ProcessEvent( intf_thread_t *p_intf, VlcProc *proc, XEvent *event )
{
    // Variables
    list<SkinWindow *>::const_iterator win;
    unsigned int msg;
    Event *evt;

    Window wnd = ((XAnyEvent *)event)->window;
    
    // Create event to dispatch in windows
    // Skin event
    if( event->type == ClientMessage )
    {
        msg = ( (XClientMessageEvent *)event )->data.l[0];
        evt = (Event *)new OSEvent( p_intf, 
            ((XAnyEvent *)event)->window, msg,
            ( (XClientMessageEvent *)event )->data.l[1],
            ( (XClientMessageEvent *)event )->data.l[2] );
    }
    // System event
    else
    {
        msg = event->type;
        evt = (Event *)new OSEvent( p_intf,
            ((XAnyEvent *)event)->window, msg, 0, (long)event );
    }

    // Process keyboard shortcuts
    if( msg == KeyPress )
    {
/*        int KeyModifier = 0;
        // If key is ALT
        if( ((GdkEventKey *)event)->state & GDK_MOD1_MASK )
        {
            KeyModifier = 1;
        }
        // If key is CTRL
        else if( ((GdkEventKey *)event)->state & GDK_CONTROL_MASK )
        {
            KeyModifier = 2;
        }
        int key = ((GdkEventKey *)event)->keyval;
        // Translate into lower case
        if( key >= 'a' && key <= 'z' )
        {
            key -= ('a' - 'A');
        }
        if( KeyModifier > 0 )
            p_intf->p_sys->p_theme->EvtBank->TestShortcut( key , KeyModifier );*/
    }

    // Send event
    else if( IsVLCEvent( msg ) )
    {
        if( !proc->EventProc( evt ) )
        {
            return 1;      // Exit VLC !
        }
    }
    else if( wnd == p_intf->p_sys->mainWin )
    {
        // Broadcast event
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
            if( wnd == ( (X11Window *)(*win) )->GetHandle() )
            {
                // Send event and check if processed
                if( (*win)->ProcessEvent( evt ) )
                {
                    delete (OSEvent *)evt;
                    return 0;
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
    
    return 0;
}

//---------------------------------------------------------------------------
// X11 interface
//---------------------------------------------------------------------------
void OSRun( intf_thread_t *p_intf )
{
    VlcProc *proc = new VlcProc( p_intf );

    Display *display = ((OSTheme *)p_intf->p_sys->p_theme)->GetDisplay();

    // Main event loop
    int close = 0;
    int count = 0;
    while( !close )
    {
        XEvent event;
        int nPending;
        XLOCK;
        nPending = XPending( display );
        XUNLOCK;
        while( !close && nPending > 0 )
        {
            XLOCK;
            XNextEvent( display, &event );
            XUNLOCK;
            close = ProcessEvent( p_intf, proc, &event );
            XLOCK;
            nPending = XPending( display );
            XUNLOCK;
        }
        
        msleep( 1000 );
        if( ++count == 100 )
        {
            count = 0;
            SkinManage( p_intf );    // Call every 100 ms
        }
    }
}
//---------------------------------------------------------------------------
bool IsVLCEvent( unsigned int msg )
{
    return( msg > VLC_MESSAGE && msg < VLC_WINDOW );
}
//---------------------------------------------------------------------------

#endif
