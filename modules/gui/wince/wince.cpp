/*****************************************************************************
 * wince.cpp: WinCE gui plugin for VLC
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
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
#include <commctrl.h>
#include <commdlg.h>

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  Open   ( vlc_object_t * );
static void Close  ( vlc_object_t * );
static void Run    ( intf_thread_t * );

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

    p_intf->p_sys->p_input = NULL;
    p_intf->p_sys->b_playing = 0;
    p_intf->p_sys->i_playing = -1;
    p_intf->p_sys->b_slider_free = 1;
    p_intf->p_sys->i_slider_pos = p_intf->p_sys->i_slider_oldpos = 0;

    p_intf->p_sys->GetOpenFile = 0;
    p_intf->p_sys->h_gsgetfile_dll = LoadLibrary( _T("gsgetfile") );
    if( p_intf->p_sys->h_gsgetfile_dll )
    {
        p_intf->p_sys->GetOpenFile = (BOOL (WINAPI *)(void *))
            GetProcAddress( p_intf->p_sys->h_gsgetfile_dll,
                            _T("gsGetOpenFileName") );
    }

    if( !p_intf->p_sys->GetOpenFile )
        p_intf->p_sys->GetOpenFile = (BOOL (WINAPI *)(void *))GetOpenFileName;

    return VLC_SUCCESS;
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

    // Unsuscribe to messages bank
    msg_Unsubscribe( p_intf, p_intf->p_sys->p_sub );

    if( p_intf->p_sys->h_gsgetfile_dll )
        FreeLibrary( p_intf->p_sys->h_gsgetfile_dll );

    // Destroy structure
    free( p_intf->p_sys );
}

/*****************************************************************************
 * Run: main loop
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    MSG msg;
    Interface intf;

    p_intf->p_sys->p_main_window = &intf;
    if( !hInstance ) hInstance = GetModuleHandle(NULL);

#ifndef UNDER_CE
    /* Initialize OLE/COM */
    CoInitialize( 0 );
#endif

    if( !intf.InitInstance( hInstance, p_intf ) ) return;

    // Main message loop
    while( GetMessage( &msg, NULL, 0, 0 ) > 0 )
    {
        TranslateMessage( &msg );
        DispatchMessage( &msg );
    }

#ifndef UNDER_CE
    /* Uninitialize OLE/COM */
    CoUninitialize();
#endif
}
