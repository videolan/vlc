/*****************************************************************************
 * win32_run.cpp:
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: win32_run.cpp,v 1.10 2003/04/28 00:18:27 ipkiss Exp $
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

//--- GENERAL ---------------------------------------------------------------
#include <wx/wx.h>
#include <windows.h>

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
#include "../src/wxdialogs.h"

// include the icon graphic
#include "share/vlc32x32.xpm"

//---------------------------------------------------------------------------
// Specific method
//---------------------------------------------------------------------------
bool IsVLCEvent( unsigned int msg );
int  SkinManage( intf_thread_t *p_intf );


//---------------------------------------------------------------------------
// Local classes declarations.
//---------------------------------------------------------------------------
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
    p_intf->p_sys->OpenDlg = new OpenDialog( p_intf, NULL, FILE_ACCESS );
    p_intf->p_sys->MessagesDlg = new Messages( p_intf, NULL );
    p_intf->p_sys->SoutDlg = new SoutDialog( p_intf, NULL );
    p_intf->p_sys->PrefsDlg = new PrefsDialog( p_intf, NULL );
    p_intf->p_sys->InfoDlg = new FileInfo( p_intf, NULL );

    vlc_mutex_lock( &p_intf->p_sys->init_lock );
    vlc_cond_signal( &p_intf->p_sys->init_cond );
    vlc_mutex_unlock( &p_intf->p_sys->init_lock );

    return TRUE;
}


//---------------------------------------------------------------------------
#if !defined(__BUILTIN__) && defined( WIN32 )
HINSTANCE hInstance = 0;
extern "C" BOOL WINAPI
DllMain (HANDLE hModule, DWORD fdwReason, LPVOID lpReserved)
{
    hInstance = (HINSTANCE)hModule;
    return TRUE;
}
#endif


//---------------------------------------------------------------------------
// Thread callback
// We create all wxWindows dialogs in a separate thread because we don't want
// any interaction with our own message loop
//---------------------------------------------------------------------------
void SkinsDialogsThread( intf_thread_t *p_intf )
{
    /* Hack to pass the p_intf pointer to the new wxWindow Instance object */
    wxTheApp = new Instance( p_intf );

#if defined( WIN32 )
#if !defined(__BUILTIN__)
    wxEntry( hInstance/*GetModuleHandle(NULL)*/, NULL, NULL, SW_SHOW, TRUE );
#else
    wxEntry( GetModuleHandle( NULL ), NULL, NULL, SW_SHOW, TRUE );
#endif
#else
    wxEntry( 1, p_args );
#endif

    return;
}


//---------------------------------------------------------------------------
// Refresh Timer Callback
//---------------------------------------------------------------------------
void CALLBACK RefreshTimer( HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime )
{
    intf_thread_t *p_intf = (intf_thread_t *)GetWindowLongPtr( hwnd,
        GWLP_USERDATA );
    SkinManage( p_intf );
}
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
// Win32 interface
//---------------------------------------------------------------------------
void OSRun( intf_thread_t *p_intf )
{
    VlcProc *Proc = new VlcProc( p_intf );
    MSG msg;
    list<SkinWindow *>::const_iterator win;
    Event *ProcessEvent;
    int KeyModifier = 0;

    // Create a new thread for wxWindows
    if( vlc_thread_create( p_intf, "Skins Dialogs Thread", SkinsDialogsThread,
                           0, 0 ) )                                                 {
        msg_Err( p_intf, "cannot create SkinsDialogsThread" );
        // Don't even enter the main loop
        return;
    }
//    vlc_mutex_lock( &p_intf->p_sys->init_lock );
//    vlc_cond_wait( &p_intf->p_sys->init_cond, &p_intf->p_sys->init_lock );
//    vlc_mutex_unlock( &p_intf->p_sys->init_lock );

     // Create refresh timer
    SetTimer( ((OSTheme *)p_intf->p_sys->p_theme)->GetParentWindow(), 42, 200,
              (TIMERPROC)RefreshTimer );

    // Compute windows message list
    while( GetMessage( &msg, NULL, 0, 0 ) )
    {

        for( win = p_intf->p_sys->p_theme->WindowList.begin();
             win != p_intf->p_sys->p_theme->WindowList.end(); win++ )
        {
            if( msg.hwnd == NULL ||
                msg.hwnd == ((Win32Window*)(*win))->GetHandle() )
            {
                break;
            }
        }
        if( win == p_intf->p_sys->p_theme->WindowList.end() )
        {
//            DispatchMessage( &msg );
//            DefWindowProc( msg.hwnd, msg.message, msg.wParam, msg.lParam );
        }

        // Translate keys
        TranslateMessage( &msg );

        // Create event
        ProcessEvent = (Event *)new OSEvent( p_intf, msg.hwnd, msg.message,
                                             msg.wParam, msg.lParam );

        /*****************************
        * Process keyboard shortcuts *
        *****************************/
        if( msg.message == WM_KEYUP )
        {
            msg_Err( p_intf, "Key : %i (%i)", msg.wParam, KeyModifier );
            // If key is CTRL
            if( msg.wParam == 17 )
                KeyModifier = 0;
            else if( KeyModifier == 0 )
            {
                p_intf->p_sys->p_theme->EvtBank->TestShortcut(
                    msg.wParam, 0 );
            }
        }
        else if( msg.message == WM_KEYDOWN )
        {
            // If key is control
            if( msg.wParam == 17 )
                KeyModifier = 2;
            else if( KeyModifier > 0 )
            {
                p_intf->p_sys->p_theme->EvtBank->TestShortcut(
                    msg.wParam, KeyModifier );
            }
        }
        else if( msg.message == WM_SYSKEYDOWN )
        {
            // If key is ALT
            if( msg.wParam == 18 )
                KeyModifier = 1;
        }
        else if( msg.message == WM_SYSKEYUP )
        {
            // If key is a system key
            KeyModifier = 0;
        }

        /************************
        * Process timer message *
        ************************/
        else if( msg.message == WM_TIMER )
        {
            DispatchMessage( &msg );
        }

        /***********************
        * VLC specific message *
        ***********************/
        else if( IsVLCEvent( msg.message ) )
        {
            if( !Proc->EventProc( ProcessEvent ) )
                break;      // Exit VLC !
        }

        /**********************
        * Broadcasted message *
        **********************/
        else if( msg.hwnd == NULL )
        {
            for( win = p_intf->p_sys->p_theme->WindowList.begin();
                 win != p_intf->p_sys->p_theme->WindowList.end(); win++ )
            {
                (*win)->ProcessEvent( ProcessEvent );
            }
        }

        /***********************
        * Process window event *
        ***********************/
        else
        {
            DispatchMessage( &msg );
        }

        // Delete event
        ProcessEvent->DestructParameters();
        delete (OSEvent *)ProcessEvent;

        // Check if vlc is closing
        Proc->IsClosing();
    }
}
//---------------------------------------------------------------------------
bool IsVLCEvent( unsigned int msg )
{
    return( msg > VLC_MESSAGE && msg < VLC_WINDOW );
}
//---------------------------------------------------------------------------

#endif
