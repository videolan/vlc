/*****************************************************************************
 * x11_run.cpp:
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: x11_run.cpp,v 1.8 2003/05/24 21:28:29 asmax Exp $
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

//--- WWWINDOWS -------------------------------------------------------------
#ifndef BASIC_SKINS
#include <wx/wx.h>
#endif

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
#ifndef BASIC_SKINS
#include "../../wxwindows/wxwindows.h"
#endif

// include the icon graphic
#include "share/vlc32x32.xpm"

#include <unistd.h>


//---------------------------------------------------------------------------
// Specific method
//---------------------------------------------------------------------------
bool IsVLCEvent( unsigned int msg );
int  SkinManage( intf_thread_t *p_intf );


//---------------------------------------------------------------------------
// Local classes declarations.
//---------------------------------------------------------------------------
#ifndef BASIC_SKINS
class Instance: public wxApp
{
public:
    Instance();
    Instance( intf_thread_t *_p_intf );

    bool OnInit();
    OpenDialog *open;

private:
    intf_thread_t *p_intf;
};
#endif


//---------------------------------------------------------------------------
// GTK2 interface
//---------------------------------------------------------------------------
/*void GTK2Proc( GdkEvent *event, gpointer data )
{
    // Get objects from data
    CallBackObjects *obj = (CallBackObjects *)data;
    VlcProc *proc        = obj->Proc;

    // Get pointer to thread info
    intf_thread_t *p_intf = proc->GetpIntf();

    // Variables
    unsigned int msg;
    Event *evt;
    list<SkinWindow *>::const_iterator win;
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

    // Process keyboard shortcuts
    if( msg == GDK_KEY_PRESS )
    {
        int KeyModifier = 0;
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
            p_intf->p_sys->p_theme->EvtBank->TestShortcut( key , KeyModifier );
    }

    // Send event
    else if( IsVLCEvent( msg ) )
    {
        if( !proc->EventProc( evt ) )
        {
            wxExit();
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

    gtk_main_do_event( event )
}*/
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// Implementation of Instance class
//---------------------------------------------------------------------------
#ifndef BASIC_SKINS
Instance::Instance( )
{
}

Instance::Instance( intf_thread_t *_p_intf )
{
    // Initialization
    p_intf = _p_intf;
}

IMPLEMENT_APP_NO_MAIN(Instance)

bool Instance::OnInit()
{
    // Set event callback. Yes, it's a big hack ;)
//    gdk_event_handler_set( GTK2Proc, (gpointer)callbackobj, NULL );

    p_intf->p_sys->p_icon = new wxIcon( vlc_xpm );
    p_intf->p_sys->OpenDlg = new OpenDialog( p_intf, NULL, FILE_ACCESS );
    p_intf->p_sys->MessagesDlg = new Messages( p_intf, NULL );
    p_intf->p_sys->SoutDlg = new SoutDialog( p_intf, NULL );
    p_intf->p_sys->PrefsDlg = new PrefsDialog( p_intf, NULL );
    p_intf->p_sys->InfoDlg = new FileInfo( p_intf, NULL );
    
    // Add timer
//    g_timeout_add( 200, (GSourceFunc)RefreshTimer, (gpointer)p_intf );

    return TRUE;
}
#endif


//---------------------------------------------------------------------------
// X11 event processing
//---------------------------------------------------------------------------
void ProcessEvent( intf_thread_t *p_intf, VlcProc *proc, XEvent *event )
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
//            wxExit();
            return;      // Exit VLC !
        }
    }
    else if( wnd == NULL )
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
            if( wnd == ( (X11Window *)(*win) )->GetHandle() )
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

}


//---------------------------------------------------------------------------
// X11 interface
//---------------------------------------------------------------------------
void OSRun( intf_thread_t *p_intf )
{
    static char  *p_args[] = { "" };

    VlcProc *proc = new VlcProc( p_intf );
    
#ifndef BASIC_SKINS
    wxTheApp = new Instance( p_intf );
    wxEntry( 1, p_args );
#endif

    Display *display = ((OSTheme *)p_intf->p_sys->p_theme)->GetDisplay();
    
    // Main event loop
    int count = 0;
    while( 1 )
    {
        XEvent event;
        while( XPending( display ) > 0 )
        {
            XNextEvent( display, &event );
            ProcessEvent( p_intf, proc, &event );
        }
        usleep( 1000 );
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
