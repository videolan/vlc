/*****************************************************************************
 * x11_run.cpp:
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: x11_run.cpp,v 1.22 2003/06/22 00:00:28 asmax Exp $
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
#include "x11_timer.h"


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
    if( event->type == ClientMessage && 
        ((XClientMessageEvent*)event)->message_type == 0 )
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
        int KeyModifier = 0;
        // If key is ALT
        if( ((XKeyEvent *)event)->state & Mod1Mask )
        {
            KeyModifier = 1;
        }
        // If key is CTRL
        else if( ((XKeyEvent *)event)->state & ControlMask )
        {
            KeyModifier = 2;
        }
        // Take the second keysym = upper case character
        int key = XLookupKeysym( (XKeyEvent *)event, 1 );
        if( KeyModifier > 0 )
            p_intf->p_sys->p_theme->EvtBank->TestShortcut( key , KeyModifier );
    }

    // Send event
    else if( IsVLCEvent( msg ) )
    {
        if( !proc->EventProc( evt ) )
        {
            delete (OSEvent *)evt;
            return 1;      // Exit VLC !
        }
    }
    else if( wnd == p_intf->p_sys->mainWin )
    {
        // Broadcast event
        for( win = SkinWindowList::Instance()->Begin();
             win != SkinWindowList::Instance()->End(); win++ )
        {
            (*win)->ProcessEvent( evt );
        }
    }
    else
    {
        // Find window matching with gwnd
        for( win = SkinWindowList::Instance()->Begin();
             win != SkinWindowList::Instance()->End(); win++ )
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


bool RefreshCallback( void *data )
{
    SkinManage( (intf_thread_t*)data );
    return True;
}


//---------------------------------------------------------------------------
// X11 interface
//---------------------------------------------------------------------------
void OSRun( intf_thread_t *p_intf )
{
    VlcProc *proc = new VlcProc( p_intf );

    Display *display = ((OSTheme *)p_intf->p_sys->p_theme)->GetDisplay();


    // Timer for SkinManage
    X11Timer *refreshTimer = new X11Timer( p_intf, 100000, RefreshCallback,
                                           (void*)p_intf );
    X11TimerManager *timerManager = X11TimerManager::Instance( p_intf );
    timerManager->addTimer( refreshTimer );

    // Main event loop
    int close = 0;
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
    }

    timerManager->Destroy();
    delete refreshTimer;
    delete proc;
}
//---------------------------------------------------------------------------
bool IsVLCEvent( unsigned int msg )
{
    return( msg > VLC_MESSAGE && msg < VLC_WINDOW );
}
//---------------------------------------------------------------------------

#endif
