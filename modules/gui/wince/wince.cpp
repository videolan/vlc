/*****************************************************************************
 * wince.cpp: WinCE gui plugin for VLC
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN (Centrale Réseaux) and its contributors
 * $Id$
 *
 * Author: Gildas Bazin <gbazin@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <vlc/intf.h>

#if defined( UNDER_CE ) && defined(__MINGW32__)
/* This is a gross hack for the wince gcc cross-compiler */
#define _off_t long
#endif

#include "wince.h"

#include <objbase.h>

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  Open   ( vlc_object_t * );
static void Close  ( vlc_object_t * );
static void Run    ( intf_thread_t * );

static int  OpenDialogs( vlc_object_t * );

static void MainLoop  ( intf_thread_t * );
static void ShowDialog( intf_thread_t *, int, int, intf_dialog_args_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define EMBED_TEXT N_("Embed video in interface")
#define EMBED_LONGTEXT N_("Embed the video inside the interface instead " \
    "of having it in a separate window.")

vlc_module_begin();
    set_description( (char *) _("WinCE interface module") );
    set_capability( "interface", 100 );
    set_callbacks( Open, Close );
    add_shortcut( "wince" );
    set_program( "wcevlc" );

    add_bool( "wince-embed", 1, NULL,
              EMBED_TEXT, EMBED_LONGTEXT, VLC_FALSE );

    add_submodule();
    set_description( _("WinCE dialogs provider") );
    set_capability( "dialogs provider", 10 );
    set_callbacks( OpenDialogs, Close );
vlc_module_end();

HINSTANCE hInstance = 0;

#if !defined(__BUILTIN__)
extern "C" BOOL WINAPI
DllMain( HANDLE hModule, DWORD fdwReason, LPVOID lpReserved )
{
    hInstance = (HINSTANCE)hModule;
    return TRUE;
}
#endif

/* Global variables used by _TOMB() / _FROMB() */
wchar_t pwsz_mbtow_wince[2048];
char psz_wtomb_wince[2048];

/*****************************************************************************
 * Open: initialize interface
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    // Check if the application is running.
    // If it's running then focus its window and bail out.
    HWND hwndMain = FindWindow( _T("VLC WinCE"), _T("VLC media player") );  
    if( hwndMain )
    {
        SetForegroundWindow( hwndMain );
        return VLC_EGENERIC;
    }

    // Allocate instance and initialize some members
    p_intf->p_sys = (intf_sys_t *)malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        msg_Err( p_intf, "out of memory" );
        return VLC_EGENERIC;
    }

    // Suscribe to messages bank
    p_intf->p_sys->p_sub = msg_Subscribe( p_intf );

    // Misc init
    p_intf->p_sys->p_audio_menu = NULL;
    p_intf->p_sys->p_video_menu = NULL;
    p_intf->p_sys->p_navig_menu = NULL;
    p_intf->p_sys->p_settings_menu = NULL;

    p_intf->pf_run = Run;
    p_intf->pf_show_dialog = NULL;

    p_intf->p_sys->p_input = NULL;
    p_intf->p_sys->b_playing = 0;
    p_intf->p_sys->i_playing = -1;
    p_intf->p_sys->b_slider_free = 1;
    p_intf->p_sys->i_slider_pos = p_intf->p_sys->i_slider_oldpos = 0;

    return VLC_SUCCESS;
}

static int OpenDialogs( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    int i_ret = Open( p_this );

    p_intf->pf_show_dialog = ShowDialog;

    return i_ret;
}

/*****************************************************************************
 * Close: destroy interface
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    if( p_intf->p_sys->p_input )
    {
        vlc_object_release( p_intf->p_sys->p_input );
    }

    MenuItemExt::ClearList( p_intf->p_sys->p_video_menu );
    delete p_intf->p_sys->p_video_menu;
    MenuItemExt::ClearList( p_intf->p_sys->p_audio_menu );
    delete p_intf->p_sys->p_audio_menu;
    MenuItemExt::ClearList( p_intf->p_sys->p_settings_menu );
    delete p_intf->p_sys->p_settings_menu;
    MenuItemExt::ClearList( p_intf->p_sys->p_navig_menu );
    delete p_intf->p_sys->p_navig_menu;

    if( p_intf->pf_show_dialog )
    {
        /* We must destroy the dialogs thread */
#if 0
        wxCommandEvent event( wxEVT_DIALOG, INTF_DIALOG_EXIT );
        p_intf->p_sys->p_wxwindow->AddPendingEvent( event );
#endif
        vlc_thread_join( p_intf );
    }

    // Unsuscribe to messages bank
    msg_Unsubscribe( p_intf, p_intf->p_sys->p_sub );

    // Destroy structure
    free( p_intf->p_sys );
}

/*****************************************************************************
 * Run: main loop
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    if( p_intf->pf_show_dialog )
    {
        /* The module is used in dialog provider mode */

        /* Create a new thread for the dialogs provider */
        if( vlc_thread_create( p_intf, "Skins Dialogs Thread",
                               MainLoop, 0, VLC_TRUE ) )
        {
            msg_Err( p_intf, "cannot create Skins Dialogs Thread" );
            p_intf->pf_show_dialog = NULL;
        }
    }
    else
    {
        /* The module is used in interface mode */
        MainLoop( p_intf );
    }
}

static void MainLoop( intf_thread_t *p_intf )
{
    MSG msg;
    Interface *intf = 0;

    if( !hInstance ) hInstance = GetModuleHandle(NULL);

    // Register window class
    WNDCLASS wc;
    wc.style = CS_HREDRAW | CS_VREDRAW ;
    wc.lpfnWndProc = (WNDPROC)CBaseWindow::BaseWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hIcon = NULL;
    wc.hInstance = hInstance;
    wc.hCursor = NULL;
    wc.hbrBackground = (HBRUSH)(COLOR_MENU+1);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = _T("VLC WinCE");
    RegisterClass( &wc );

#ifndef UNDER_CE
    /* Initialize OLE/COM */
    CoInitialize( 0 );
#endif

    if( !p_intf->pf_show_dialog )
    {
        /* The module is used in interface mode */
        p_intf->p_sys->p_window = intf = new Interface( p_intf, 0, hInstance );

        /* Create/Show the interface */
        if( !intf->InitInstance() ) goto end;
    }

    /* Creates the dialogs provider */
    p_intf->p_sys->p_window =
        CreateDialogsProvider( p_intf, p_intf->pf_show_dialog ?
                               NULL : p_intf->p_sys->p_window, hInstance );

    p_intf->p_sys->pf_show_dialog = ShowDialog;

    /* OK, initialization is over */
    vlc_thread_ready( p_intf );

    /* Check if we need to start playing */
    if( !p_intf->pf_show_dialog && p_intf->b_play )
    {
        playlist_t *p_playlist =
            (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                           FIND_ANYWHERE );
        if( p_playlist )
        {
            playlist_Play( p_playlist );
            vlc_object_release( p_playlist );
        }
    }

    // Main message loop
    while( GetMessage( &msg, NULL, 0, 0 ) > 0 )
    {
        TranslateMessage( &msg );
        DispatchMessage( &msg );
    }

 end:
    if( intf ) delete intf;

#ifndef UNDER_CE
    /* Uninitialize OLE/COM */
    CoUninitialize();
#endif
}

/*****************************************************************************
 * CBaseWindow Implementation
 *****************************************************************************/
LRESULT CALLBACK CBaseWindow::BaseWndProc( HWND hwnd, UINT msg, WPARAM wParam,
                                           LPARAM lParam )
{
    CBaseWindow *p_obj;

    // check to see if a copy of the 'this' pointer needs to be saved
    if( msg == WM_CREATE )
    {
        p_obj = (CBaseWindow *)(((LPCREATESTRUCT)lParam)->lpCreateParams);
        SetWindowLong( hwnd, GWL_USERDATA,
                       (LONG)((LPCREATESTRUCT)lParam)->lpCreateParams );

        p_obj->hWnd = hwnd;
    }

    if( msg == WM_INITDIALOG )
    {
        p_obj = (CBaseWindow *)lParam;
        SetWindowLong( hwnd, GWL_USERDATA, lParam );
        p_obj->hWnd = hwnd;
    }

    // Retrieve the pointer
    p_obj = (CBaseWindow *)GetWindowLong( hwnd, GWL_USERDATA );

    if( !p_obj ) return DefWindowProc( hwnd, msg, wParam, lParam );

    // Filter message through child classes
    return p_obj->WndProc( hwnd, msg, wParam, lParam );
}

int CBaseWindow::CreateDialogBox( HWND hwnd, CBaseWindow *p_obj )
{
    uint8_t p_buffer[sizeof(DLGTEMPLATE) + sizeof(WORD) * 4];
    DLGTEMPLATE *p_dlg_template = (DLGTEMPLATE *)p_buffer;
    memset( p_dlg_template, 0, sizeof(DLGTEMPLATE) + sizeof(WORD) * 4 );

    // these values are arbitrary, they won't be used normally anyhow
    p_dlg_template->x  = 0; p_dlg_template->y  = 0;
    p_dlg_template->cx = 300; p_dlg_template->cy = 300;
    p_dlg_template->style =
        DS_MODALFRAME|WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_SIZEBOX;

    return DialogBoxIndirectParam( GetModuleHandle(0), p_dlg_template, hwnd,
                                   (DLGPROC)p_obj->BaseWndProc, (LPARAM)p_obj);
}

/*****************************************************************************
 * ShowDialog
 *****************************************************************************/
static void ShowDialog( intf_thread_t *p_intf, int i_dialog_event, int i_arg,
                        intf_dialog_args_t *p_arg )
{
    SendMessage( p_intf->p_sys->p_window->GetHandle(), WM_CANCELMODE, 0, 0 );
    if( i_dialog_event == INTF_DIALOG_POPUPMENU && i_arg == 0 ) return;

    /* Hack to prevent popup events to be enqueued when
     * one is already active */
#if 0
    if( i_dialog_event != INTF_DIALOG_POPUPMENU ||
        !p_intf->p_sys->p_popup_menu )
#endif
    {
        SendMessage( p_intf->p_sys->p_window->GetHandle(),
                     WM_APP + i_dialog_event, (WPARAM)i_arg, (LPARAM)p_arg );
    }
}
