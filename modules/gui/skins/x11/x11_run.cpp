/*****************************************************************************
 * x11_run.cpp:
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: x11_run.cpp,v 1.13 2003/06/01 22:11:24 asmax Exp $
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

//--- WXWINDOWS -------------------------------------------------------------
#ifndef BASIC_SKINS
/* Let vlc take care of the i18n stuff */
#define WXINTL_NO_GETTEXT_MACRO
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

//---------------------------------------------------------------------------
// Specific method
//---------------------------------------------------------------------------
bool IsVLCEvent( unsigned int msg );
int  SkinManage( intf_thread_t *p_intf );


#ifndef BASIC_SKINS
//---------------------------------------------------------------------------
// Local classes declarations.
//---------------------------------------------------------------------------
class Instance: public wxApp
{
public:
    Instance();
    Instance( intf_thread_t *_p_intf );

    bool OnInit();
    int  OnExit();
    OpenDialog *open;

private:
    intf_thread_t *p_intf;
};

//---------------------------------------------------------------------------
// Implementation of Instance class
//---------------------------------------------------------------------------
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
    p_intf->p_sys->p_icon = new wxIcon( vlc_xpm );

    // Create all the dialog boxes
    p_intf->p_sys->OpenDlg = new OpenDialog( p_intf, NULL, FILE_ACCESS );
    p_intf->p_sys->MessagesDlg = new Messages( p_intf, NULL );
    p_intf->p_sys->SoutDlg = new SoutDialog( p_intf, NULL );
    p_intf->p_sys->PrefsDlg = new PrefsDialog( p_intf, NULL );
    p_intf->p_sys->InfoDlg = new FileInfo( p_intf, NULL );

    // OK, initialization is over, now the other thread can go on working...
    vlc_thread_ready( p_intf );

    return TRUE;
}

int Instance::OnExit()
{
    // Delete evertything
    delete p_intf->p_sys->InfoDlg;
    delete p_intf->p_sys->PrefsDlg;
    delete p_intf->p_sys->SoutDlg;
    delete p_intf->p_sys->MessagesDlg;
    delete p_intf->p_sys->OpenDlg;
    delete p_intf->p_sys->p_icon;

    return 0;
}

//---------------------------------------------------------------------------
// Thread callback
// We create all wxWindows dialogs in a separate thread because we don't want
// any interaction with our own message loop
//---------------------------------------------------------------------------
void SkinsDialogsThread( intf_thread_t *p_intf )
{
    /* Hack to pass the p_intf pointer to the new wxWindow Instance object */
    wxTheApp = new Instance( p_intf );

    static char  *p_args[] = { "" };
    wxEntry( 1, p_args );

    return;
}

#endif // WX_SKINS

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
#ifndef BASIC_SKINS
            wxExit();
#endif
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

#ifndef BASIC_SKINS
    // Create a new thread for wxWindows
    if( vlc_thread_create( p_intf, "Skins Dialogs Thread", SkinsDialogsThread,
                           0, VLC_TRUE ) )
    {
        msg_Err( p_intf, "cannot create SkinsDialogsThread" );
        // Don't even enter the main loop
        return;
    }
#endif
    
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
