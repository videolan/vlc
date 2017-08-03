/*****************************************************************************
 * win32.c: Global-Hotkey _WIN32 handling for vlc
 *****************************************************************************
 * Copyright (C) 2008-2009 the VideoLAN team
 *
 * Authors: Domani Hannes <ssbssa at yahoo dot de>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <ctype.h>

#define VLC_MODULE_LICENSE VLC_LICENSE_GPL_2_PLUS
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_actions.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Open( vlc_object_t *p_this );
static void Close( vlc_object_t *p_this );
static void *Thread( void *p_data );
LRESULT CALLBACK WMHOTKEYPROC( HWND, UINT, WPARAM, LPARAM );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin()
    set_shortname( N_("Global Hotkeys") )
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_HOTKEYS )
    set_description( N_("Global Hotkeys interface") )
    set_capability( "interface", 0 )
    set_callbacks( Open, Close )
    add_shortcut( "globalhotkeys" )
vlc_module_end()

struct intf_sys_t
{
    vlc_thread_t thread;
    HWND hotkeyWindow;
    vlc_mutex_t lock;
    vlc_cond_t wait;
};

/*****************************************************************************
 * Open: initialize interface
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t *p_sys = malloc( sizeof (intf_sys_t) );

    if( p_sys == NULL )
        return VLC_ENOMEM;

    p_intf->p_sys = p_sys;
    p_sys->hotkeyWindow = NULL;
    vlc_mutex_init( &p_sys->lock );
    vlc_cond_init( &p_sys->wait );

    if( vlc_clone( &p_sys->thread, Thread, p_intf, VLC_THREAD_PRIORITY_LOW ) )
    {
        vlc_mutex_destroy( &p_sys->lock );
        vlc_cond_destroy( &p_sys->wait );
        free( p_sys );
        p_intf->p_sys = NULL;

        return VLC_ENOMEM;
    }

    vlc_mutex_lock( &p_sys->lock );
    while( p_sys->hotkeyWindow == NULL )
        vlc_cond_wait( &p_sys->wait, &p_sys->lock );
    if( p_sys->hotkeyWindow == INVALID_HANDLE_VALUE )
    {
        vlc_mutex_unlock( &p_sys->lock );
        vlc_join( p_sys->thread, NULL );
        vlc_mutex_destroy( &p_sys->lock );
        vlc_cond_destroy( &p_sys->wait );
        free( p_sys );
        p_intf->p_sys = NULL;

        return VLC_ENOMEM;
    }
    vlc_mutex_unlock( &p_sys->lock );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t *p_sys = p_intf->p_sys;

    /* stop hotkey window */
    vlc_mutex_lock( &p_sys->lock );
    if( p_sys->hotkeyWindow != NULL )
        PostMessage( p_sys->hotkeyWindow, WM_CLOSE, 0, 0 );
    vlc_mutex_unlock( &p_sys->lock );

    vlc_join( p_sys->thread, NULL );
    vlc_mutex_destroy( &p_sys->lock );
    vlc_cond_destroy( &p_sys->wait );
    free( p_sys );
}

/*****************************************************************************
 * Thread: main loop
 *****************************************************************************/
static void *Thread( void *p_data )
{
    MSG message;

    intf_thread_t *p_intf = p_data;
    intf_sys_t *p_sys = p_intf->p_sys;

    /* Window which receives Hotkeys */
    vlc_mutex_lock( &p_sys->lock );
    p_sys->hotkeyWindow =
        (void*)CreateWindow( _T("STATIC"),           /* name of window class */
                _T("VLC ghk ") _T(VERSION),         /* window title bar text */
                0,                                           /* window style */
                0,                                   /* default X coordinate */
                0,                                   /* default Y coordinate */
                0,                                           /* window width */
                0,                                          /* window height */
                NULL,                                    /* no parent window */
                NULL,                              /* no menu in this window */
                GetModuleHandle(NULL),    /* handle of this program instance */
                NULL );                                 /* sent to WM_CREATE */

    if( p_sys->hotkeyWindow == NULL )
    {
        p_sys->hotkeyWindow = INVALID_HANDLE_VALUE;
        vlc_cond_signal( &p_sys->wait );
        vlc_mutex_unlock( &p_sys->lock );
        return NULL;
    }
    vlc_cond_signal( &p_sys->wait );
    vlc_mutex_unlock( &p_sys->lock );

    SetWindowLongPtr( p_sys->hotkeyWindow, GWLP_WNDPROC,
            (LONG_PTR)WMHOTKEYPROC );
    SetWindowLongPtr( p_sys->hotkeyWindow, GWLP_USERDATA,
            (LONG_PTR)p_intf );

    /* Registering of Hotkeys */
    for( const char* const* ppsz_keys = vlc_actions_get_key_names( p_intf );
         *ppsz_keys != NULL; ppsz_keys++ )
    {
        uint_fast32_t *p_keys;
        size_t i_nb_keys = vlc_actions_get_keycodes( p_intf, *ppsz_keys, true,
                                                     &p_keys );
        for( size_t i = 0; i < i_nb_keys; ++i )
        {
            uint_fast32_t i_key = p_keys[i];
            UINT i_keyMod = 0;
            if( i_key & KEY_MODIFIER_SHIFT ) i_keyMod |= MOD_SHIFT;
            if( i_key & KEY_MODIFIER_ALT ) i_keyMod |= MOD_ALT;
            if( i_key & KEY_MODIFIER_CTRL ) i_keyMod |= MOD_CONTROL;

#define HANDLE( key ) case KEY_##key: i_vk = VK_##key; break
#define HANDLE2( key, key2 ) case KEY_##key: i_vk = VK_##key2; break

#define KEY_SPACE ' '

#ifndef VK_VOLUME_DOWN
#define VK_VOLUME_DOWN          0xAE
#define VK_VOLUME_UP            0xAF
#endif

#ifndef VK_MEDIA_NEXT_TRACK
#define VK_MEDIA_NEXT_TRACK     0xB0
#define VK_MEDIA_PREV_TRACK     0xB1
#define VK_MEDIA_STOP           0xB2
#define VK_MEDIA_PLAY_PAUSE     0xB3
#endif

#ifndef VK_PAGEUP
#define VK_PAGEUP               0x21
#define VK_PAGEDOWN             0x22
#endif

            UINT i_vk = 0;
            switch( i_key & ~KEY_MODIFIER )
            {
                HANDLE( LEFT );
                HANDLE( RIGHT );
                HANDLE( UP );
                HANDLE( DOWN );
                HANDLE( SPACE );
                HANDLE2( ESC, ESCAPE );
                HANDLE2( ENTER, RETURN );
                HANDLE( F1 );
                HANDLE( F2 );
                HANDLE( F3 );
                HANDLE( F4 );
                HANDLE( F5 );
                HANDLE( F6 );
                HANDLE( F7 );
                HANDLE( F8 );
                HANDLE( F9 );
                HANDLE( F10 );
                HANDLE( F11 );
                HANDLE( F12 );
                HANDLE( PAGEUP );
                HANDLE( PAGEDOWN );
                HANDLE( HOME );
                HANDLE( END );
                HANDLE( INSERT );
                HANDLE( DELETE );
                HANDLE( VOLUME_DOWN );
                HANDLE( VOLUME_UP );
                HANDLE( MEDIA_PLAY_PAUSE );
                HANDLE( MEDIA_STOP );
                HANDLE( MEDIA_PREV_TRACK );
                HANDLE( MEDIA_NEXT_TRACK );

                default:
                    i_vk = toupper( (uint8_t)(i_key & ~KEY_MODIFIER) );
                    break;
            }
            if( !i_vk ) continue;

#undef HANDLE
#undef HANDLE2

            ATOM atom = GlobalAddAtomA( *ppsz_keys );
            if( !atom ) continue;

            if( !RegisterHotKey( p_sys->hotkeyWindow, atom, i_keyMod, i_vk ) )
                GlobalDeleteAtom( atom );
        }
        free( p_keys );
    }

    /* Main message loop */
    while( GetMessage( &message, NULL, 0, 0 ) )
        DispatchMessage( &message );

    /* Unregistering of Hotkeys */
    for( const char* const* ppsz_keys = vlc_actions_get_key_names( p_intf );
         *ppsz_keys != NULL; ppsz_keys++ )
    {
        ATOM atom = GlobalFindAtomA( *ppsz_keys );
        if( !atom ) continue;

        if( UnregisterHotKey( p_sys->hotkeyWindow, atom ) )
            GlobalDeleteAtom( atom );
    }

    /* close window */
    vlc_mutex_lock( &p_sys->lock );
    DestroyWindow( p_sys->hotkeyWindow );
    p_sys->hotkeyWindow = NULL;
    vlc_mutex_unlock( &p_sys->lock );

    return NULL;
}

/*****************************************************************************
 * WMHOTKEYPROC: event callback
 *****************************************************************************/
LRESULT CALLBACK WMHOTKEYPROC( HWND hwnd, UINT uMsg, WPARAM wParam,
        LPARAM lParam )
{
    switch( uMsg )
    {
        case WM_HOTKEY:
            {
                char psz_atomName[44];

                LONG_PTR ret = GetWindowLongPtr( hwnd, GWLP_USERDATA );
                intf_thread_t *p_intf = (intf_thread_t*)ret;
                strcpy( psz_atomName, "key-" );

                if( !GlobalGetAtomNameA(
                        wParam, psz_atomName + 4,
                        sizeof( psz_atomName ) - 4 ) )
                    return 0;

                /* search for key associated with VLC */
                vlc_action_id_t action = vlc_actions_get_id( psz_atomName );
                if( action != ACTIONID_NONE )
                {
                    var_SetInteger( p_intf->obj.libvlc,
                            "key-action", action );
                    return 1;
                }
            }
            break;

        case WM_DESTROY:
            PostQuitMessage( 0 );
            break;

        default:
            return DefWindowProc( hwnd, uMsg, wParam, lParam );
    }

    return 0;
}
