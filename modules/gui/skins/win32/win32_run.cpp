/*****************************************************************************
 * win32_run.cpp:
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: win32_run.cpp,v 1.3 2003/03/20 09:29:07 karibu Exp $
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


//--- WIN32 -----------------------------------------------------------------
#include <windows.h>

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
    list<Window *>::const_iterator win;
    Event *ProcessEvent;
    int KeyModifier = 0;

    // Create refresh timer
    SetTimer( ((OSTheme *)p_intf->p_sys->p_theme)->GetParentWindow(), 42, 200,
              (TIMERPROC)RefreshTimer );

    // Compute windows message list
    while( GetMessage( &msg, NULL, 0, 0 ) )
    {

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
                p_intf->p_sys->p_theme->EvtBank->TestShortcut(
                    msg.wParam, KeyModifier );
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
        * Broadcsated message *
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


