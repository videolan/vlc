/*****************************************************************************
 * specific.c: Win32 specific initilization
 *****************************************************************************
 * Copyright (C) 2001-2004, 2010 VLC authors and VideoLAN
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define UNICODE
#include <vlc_common.h>
#include "../libvlc.h"
#include <vlc_playlist.h>
#include <vlc_url.h>

#include "../config/vlc_getopt.h"

#include <mmsystem.h>
#include <winsock.h>


static int system_InitWSA(int hi, int lo)
{
    WSADATA data;

    if (WSAStartup(MAKEWORD(hi, lo), &data) == 0)
    {
        if (LOBYTE(data.wVersion) == 2 && HIBYTE(data.wVersion) == 2)
            return 0;
        /* Winsock DLL is not usable */
        WSACleanup( );
    }
    return -1;
}

/**
 * Initializes MME timer, Winsock.
 */
void system_Init(void)
{
#if !VLC_WINSTORE_APP
    timeBeginPeriod(5);
#endif

    if (system_InitWSA(2, 2) && system_InitWSA(1, 1))
        fputs("Error: cannot initialize Winsocks\n", stderr);
}

/*****************************************************************************
 * system_Configure: check for system specific configuration options.
 *****************************************************************************/
static unsigned __stdcall IPCHelperThread( void * );
LRESULT CALLBACK WMCOPYWNDPROC( HWND, UINT, WPARAM, LPARAM );
static vlc_object_t *p_helper = NULL;
static unsigned long hIPCHelper;
static HANDLE hIPCHelperReady;

typedef struct
{
    int argc;
    int enqueue;
    char data[];
} vlc_ipc_data_t;

void system_Configure( libvlc_int_t *p_this, int i_argc, const char *const ppsz_argv[] )
{
#if !VLC_WINSTORE_APP
    /* Raise default priority of the current process */
#ifndef ABOVE_NORMAL_PRIORITY_CLASS
#   define ABOVE_NORMAL_PRIORITY_CLASS 0x00008000
#endif
    if( var_InheritBool( p_this, "high-priority" ) )
    {
        if( SetPriorityClass( GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS )
             || SetPriorityClass( GetCurrentProcess(), HIGH_PRIORITY_CLASS ) )
        {
            msg_Dbg( p_this, "raised process priority" );
        }
        else
        {
            msg_Dbg( p_this, "could not raise process priority" );
        }
    }

    if( var_InheritBool( p_this, "one-instance" )
     || ( var_InheritBool( p_this, "one-instance-when-started-from-file" )
       && var_InheritBool( p_this, "started-from-file" ) ) )
    {
        HANDLE hmutex;

        msg_Info( p_this, "one instance mode ENABLED");

        /* Use a named mutex to check if another instance is already running */
        if( !( hmutex = CreateMutex( 0, TRUE, L"VLC ipc "VERSION ) ) )
        {
            /* Failed for some reason. Just ignore the option and go on as
             * normal. */
            msg_Err( p_this, "one instance mode DISABLED "
                     "(mutex couldn't be created)" );
            return;
        }

        if( GetLastError() != ERROR_ALREADY_EXISTS )
        {
            /* We are the 1st instance. */
            p_helper =
                vlc_custom_create( p_this, sizeof(*p_helper), "ipc helper" );

            /* Run the helper thread */
            hIPCHelperReady = CreateEvent( NULL, FALSE, FALSE, NULL );
            hIPCHelper = _beginthreadex( NULL, 0, IPCHelperThread, p_helper,
                                         0, NULL );
            if( hIPCHelper )
                WaitForSingleObject( hIPCHelperReady, INFINITE );
            else
            {
                msg_Err( p_this, "one instance mode DISABLED "
                         "(IPC helper thread couldn't be created)" );
                vlc_object_release (p_helper);
                p_helper = NULL;
            }
            CloseHandle( hIPCHelperReady );

            /* Initialization done.
             * Release the mutex to unblock other instances */
            ReleaseMutex( hmutex );
        }
        else
        {
            /* Another instance is running */

            HWND ipcwindow;

            /* Wait until the 1st instance is initialized */
            WaitForSingleObject( hmutex, INFINITE );

            /* Locate the window created by the IPC helper thread of the
             * 1st instance */
            if( !( ipcwindow = FindWindow( 0, L"VLC ipc "VERSION ) ) )
            {
                msg_Err( p_this, "one instance mode DISABLED "
                         "(couldn't find 1st instance of program)" );
                ReleaseMutex( hmutex );
                return;
            }

            /* We assume that the remaining parameters are filenames
             * and their input options */
            if( i_argc > 0 )
            {
                COPYDATASTRUCT wm_data;
                int i_opt;
                vlc_ipc_data_t *p_data;
                size_t i_data = sizeof (*p_data);

                for( i_opt = 0; i_opt < i_argc; i_opt++ )
                {
                    i_data += sizeof (size_t);
                    i_data += strlen( ppsz_argv[ i_opt ] ) + 1;
                }

                p_data = malloc( i_data );
                p_data->argc = i_argc;
                p_data->enqueue = var_InheritBool( p_this, "playlist-enqueue" );
                i_data = 0;
                for( i_opt = 0; i_opt < i_argc; i_opt++ )
                {
                    size_t i_len = strlen( ppsz_argv[ i_opt ] ) + 1;
                    /* Windows will never switch to an architecture
                     * with stronger alignment requirements, right. */
                    *((size_t *)(p_data->data + i_data)) = i_len;
                    i_data += sizeof (size_t);
                    memcpy( &p_data->data[i_data], ppsz_argv[ i_opt ], i_len );
                    i_data += i_len;
                }
                i_data += sizeof (*p_data);

                /* Send our playlist items to the 1st instance */
                wm_data.dwData = 0;
                wm_data.cbData = i_data;
                wm_data.lpData = p_data;
                SendMessage( ipcwindow, WM_COPYDATA, 0, (LPARAM)&wm_data );
            }

            /* Initialization done.
             * Release the mutex to unblock other instances */
            ReleaseMutex( hmutex );

            /* Bye bye */
            system_End( );
            exit( 0 );
        }
    }
#endif
}

#if !VLC_WINSTORE_APP
static unsigned __stdcall IPCHelperThread( void *data )
{
    vlc_object_t *p_this = data;
    HWND ipcwindow;
    MSG message;

    ipcwindow =
        CreateWindow( L"STATIC",                     /* name of window class */
                  L"VLC ipc "VERSION,               /* window title bar text */
                  0,                                         /* window style */
                  0,                                 /* default X coordinate */
                  0,                                 /* default Y coordinate */
                  0,                                         /* window width */
                  0,                                        /* window height */
                  NULL,                                  /* no parent window */
                  NULL,                            /* no menu in this window */
                  GetModuleHandle(NULL),  /* handle of this program instance */
                  NULL );                               /* sent to WM_CREATE */

    SetWindowLongPtr( ipcwindow, GWLP_WNDPROC, (LRESULT)WMCOPYWNDPROC );
    SetWindowLongPtr( ipcwindow, GWLP_USERDATA, (LONG_PTR)p_this );

    /* Signal the creation of the thread and events queue */
    SetEvent( hIPCHelperReady );

    while( GetMessage( &message, NULL, 0, 0 ) )
    {
        TranslateMessage( &message );
        DispatchMessage( &message );
    }
    return 0;
}

LRESULT CALLBACK WMCOPYWNDPROC( HWND hwnd, UINT uMsg, WPARAM wParam,
                                LPARAM lParam )
{
    if( uMsg == WM_QUIT  )
    {
        PostQuitMessage( 0 );
    }
    else if( uMsg == WM_COPYDATA )
    {
        COPYDATASTRUCT *pwm_data = (COPYDATASTRUCT*)lParam;
        vlc_object_t *p_this;
        playlist_t *p_playlist;

        p_this = (vlc_object_t *)
            (uintptr_t)GetWindowLongPtr( hwnd, GWLP_USERDATA );

        if( !p_this ) return 0;

        /* Add files to the playlist */
        p_playlist = pl_Get( p_this );

        if( pwm_data->lpData )
        {
            char **ppsz_argv;
            vlc_ipc_data_t *p_data = (vlc_ipc_data_t *)pwm_data->lpData;
            size_t i_data = 0;
            int i_argc = p_data->argc, i_opt, i_options;

            ppsz_argv = (char **)malloc( i_argc * sizeof(char *) );
            for( i_opt = 0; i_opt < i_argc; i_opt++ )
            {
                ppsz_argv[i_opt] = p_data->data + i_data + sizeof(size_t);
                i_data += sizeof(size_t) + *((size_t *)(p_data->data + i_data));
            }

            for( i_opt = 0; i_opt < i_argc; i_opt++ )
            {
                i_options = 0;

                /* Count the input options */
                while( i_opt + i_options + 1 < i_argc &&
                        *ppsz_argv[ i_opt + i_options + 1 ] == ':' )
                {
                    i_options++;
                }

#warning URI conversion must be done in calling process instead!
                /* FIXME: This breaks relative paths if calling vlc.exe is
                 * started from a different working directory. */
                char *psz_URI = NULL;
                if( strstr( ppsz_argv[i_opt], "://" ) == NULL )
                    psz_URI = vlc_path2uri( ppsz_argv[i_opt], NULL );
                playlist_AddExt( p_playlist,
                        (psz_URI != NULL) ? psz_URI : ppsz_argv[i_opt],
                        NULL, PLAYLIST_APPEND |
                        ( ( i_opt || p_data->enqueue ) ? 0 : PLAYLIST_GO ),
                        PLAYLIST_END, -1,
                        i_options,
                        (char const **)( i_options ? &ppsz_argv[i_opt+1] : NULL ),
                        VLC_INPUT_OPTION_TRUSTED,
                        true, pl_Unlocked );

                i_opt += i_options;
                free( psz_URI );
            }

            free( ppsz_argv );
        }
    }

    return DefWindowProc( hwnd, uMsg, wParam, lParam );
}
#endif

/**
 * Cleans up after system_Init() and system_Configure().
 */
void system_End(void)
{
#if !VLC_WINSTORE_APP
    HWND ipcwindow;

    /* FIXME: thread-safety... */
    if (p_helper)
    {
        if( ( ipcwindow = FindWindow( 0, L"VLC ipc "VERSION ) ) != 0 )
        {
            SendMessage( ipcwindow, WM_QUIT, 0, 0 );
        }
        vlc_object_release (p_helper);
        p_helper = NULL;
    }

    timeEndPeriod(5);
#endif

    /* XXX: In theory, we should not call this if WSAStartup() failed. */
    WSACleanup();
}
