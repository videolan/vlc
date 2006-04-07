/*****************************************************************************
 * win32_specific.c: Win32 specific features
 *****************************************************************************
 * Copyright (C) 2001-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@videolan.org>
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
#include <string.h>                                              /* strdup() */
#include <stdlib.h>                                                /* free() */

#include <vlc/vlc.h>
#include <vlc/input.h>
#include "vlc_playlist.h"

#ifdef WIN32                       /* optind, getopt(), included in unistd.h */
#   include "../extras/getopt.h"
#endif

#if !defined( UNDER_CE )
#   include <io.h>
#   include <fcntl.h>
#endif

#include <winsock.h>

/*****************************************************************************
 * system_Init: initialize winsock and misc other things.
 *****************************************************************************/
void system_Init( vlc_t *p_this, int *pi_argc, char *ppsz_argv[] )
{
    WSADATA Data;

    /* Get our full path */
    char psz_path[MAX_PATH];
    char *psz_vlc;

#if defined( UNDER_CE )
    wchar_t psz_wpath[MAX_PATH];
    if( GetModuleFileName( NULL, psz_wpath, MAX_PATH ) )
    {
        WideCharToMultiByte( CP_ACP, 0, psz_wpath, -1,
                             psz_path, MAX_PATH, NULL, NULL );
    }
    else psz_path[0] = '\0';

#else
    if( ppsz_argv[0] )
    {
        GetFullPathName( ppsz_argv[0], MAX_PATH, psz_path, &psz_vlc );
    }
    else if( !GetModuleFileName( NULL, psz_path, MAX_PATH ) )
    {
        psz_path[0] = '\0';
    }
#endif

    if( (psz_vlc = strrchr( psz_path, '\\' )) ) *psz_vlc = '\0';

    p_this->p_libvlc->psz_vlcpath = strdup( psz_path );

    /* Set the default file-translation mode */
#if !defined( UNDER_CE )
    _fmode = _O_BINARY;
    _setmode( _fileno( stdin ), _O_BINARY ); /* Needed for pipes */
#endif

    /* Call mdate() once to make sure it is initialized properly */
    mdate();

    /* WinSock Library Init. */
    if( !WSAStartup( MAKEWORD( 2, 0 ), &Data ) )
    {
        /* Confirm that the WinSock DLL supports 2.0.*/
        if( LOBYTE( Data.wVersion ) != 2 || HIBYTE( Data.wVersion ) != 0 )
        {
            /* We could not find a suitable WinSock DLL. */
            WSACleanup( );
        }
        else
        {
            /* Everything went ok. */
            return;
        }
    }

    /* Let's try with WinSock 1.1 */
    if( !WSAStartup( MAKEWORD( 1, 1 ), &Data ) )
    {
        /* Confirm that the WinSock DLL supports 1.1.*/
        if( LOBYTE( Data.wVersion ) != 1 || HIBYTE( Data.wVersion ) != 1 )
        {
            /* We could not find a suitable WinSock DLL. */
            WSACleanup( );
        }
        else
        {
            /* Everything went ok. */
            return;
        }
    }

    fprintf( stderr, "error: can't initialize WinSocks\n" );

    return;
}

/*****************************************************************************
 * system_Configure: check for system specific configuration options.
 *****************************************************************************/
static void IPCHelperThread( vlc_object_t * );
LRESULT CALLBACK WMCOPYWNDPROC( HWND, UINT, WPARAM, LPARAM );

void system_Configure( vlc_t *p_this, int *pi_argc, char *ppsz_argv[] )
{
#if !defined( UNDER_CE )
    p_this->p_libvlc->b_fast_mutex = config_GetInt( p_this, "fast-mutex" );
    p_this->p_libvlc->i_win9x_cv = config_GetInt( p_this, "win9x-cv-method" );

    /* Raise default priority of the current process */
#ifndef ABOVE_NORMAL_PRIORITY_CLASS
#   define ABOVE_NORMAL_PRIORITY_CLASS 0x00008000
#endif
    if( config_GetInt( p_this, "high-priority" ) )
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

    if( config_GetInt( p_this, "one-instance" )
        || ( config_GetInt( p_this, "one-instance-when-started-from-file" )
             && config_GetInt( p_this, "started-from-file" ) ) )
    {
        HANDLE hmutex;

        msg_Info( p_this, "one instance mode ENABLED");

        /* Use a named mutex to check if another instance is already running */
        if( !( hmutex = CreateMutex( 0, TRUE, _T("VLC ipc ") _T(VERSION) ) ) )
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
            vlc_object_t *p_helper =
             (vlc_object_t *)vlc_object_create( p_this, sizeof(vlc_object_t) );

            /* Run the helper thread */
            if( vlc_thread_create( p_helper, "IPC helper", IPCHelperThread,
                                   VLC_THREAD_PRIORITY_LOW, VLC_TRUE ) )
            {
                msg_Err( p_this, "one instance mode DISABLED "
                         "(IPC helper thread couldn't be created)" );

            }

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
            if( !( ipcwindow = FindWindow( 0, _T("VLC ipc ") _T(VERSION) ) ) )
            {
                msg_Err( p_this, "one instance mode DISABLED "
                         "(couldn't find 1st instance of program)" );
                ReleaseMutex( hmutex );
                return;
            }

            /* We assume that the remaining parameters are filenames
             * and their input options */
            if( *pi_argc - 1 >= optind )
            {
                COPYDATASTRUCT wm_data;
                int i_opt, i_data;
                char *p_data;

                i_data = sizeof(int);
                for( i_opt = optind; i_opt < *pi_argc; i_opt++ )
                {
                    i_data += sizeof(int);
                    i_data += strlen( ppsz_argv[ i_opt ] ) + 1;
                }

                p_data = (char *)malloc( i_data );
                *((int *)&p_data[0]) = *pi_argc - optind;
                i_data = sizeof(int);
                for( i_opt = optind; i_opt < *pi_argc; i_opt++ )
                {
                    int i_len = strlen( ppsz_argv[ i_opt ] ) + 1;
                    *((int *)&p_data[i_data]) = i_len;
                    i_data += sizeof(int);
                    memcpy( &p_data[i_data], ppsz_argv[ i_opt ], i_len );
                    i_data += i_len;
                }

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
            system_End( p_this );
            exit( 0 );
        }
    }

#endif
}

static void IPCHelperThread( vlc_object_t *p_this )
{
    HWND ipcwindow;
    MSG message;

    ipcwindow =
        CreateWindow( _T("STATIC"),                  /* name of window class */
                  _T("VLC ipc ") _T(VERSION),       /* window title bar text */
                  0,                                         /* window style */
                  0,                                 /* default X coordinate */
                  0,                                 /* default Y coordinate */
                  0,                                         /* window width */
                  0,                                        /* window height */
                  NULL,                                  /* no parent window */
                  NULL,                            /* no menu in this window */
                  GetModuleHandle(NULL),  /* handle of this program instance */
                  NULL );                               /* sent to WM_CREATE */

    SetWindowLong( ipcwindow, GWL_WNDPROC, (LONG)WMCOPYWNDPROC );
    SetWindowLong( ipcwindow, GWL_USERDATA, (LONG)p_this );

    /* Signal the creation of the thread and events queue */
    vlc_thread_ready( p_this );

    while( GetMessage( &message, NULL, 0, 0 ) )
    {
        TranslateMessage( &message );
        DispatchMessage( &message );
    }
}

LRESULT CALLBACK WMCOPYWNDPROC( HWND hwnd, UINT uMsg, WPARAM wParam,
                                LPARAM lParam )
{
    if( uMsg == WM_COPYDATA )
    {
        COPYDATASTRUCT *pwm_data = (COPYDATASTRUCT*)lParam;
        vlc_object_t *p_this;
        playlist_t *p_playlist;

        p_this = (vlc_object_t *)GetWindowLong( hwnd, GWL_USERDATA );

        if( !p_this ) return 0;

        /* Add files to the playlist */
        p_playlist = (playlist_t *)vlc_object_find( p_this,
                                                    VLC_OBJECT_PLAYLIST,
                                                    FIND_ANYWHERE );
        if( !p_playlist ) return 0;

        if( pwm_data->lpData )
        {
            int i_argc, i_data, i_opt, i_options;
            char **ppsz_argv;
            char *p_data = (char *)pwm_data->lpData;

            i_argc = *((int *)&p_data[0]);
            ppsz_argv = (char **)malloc( i_argc * sizeof(char *) );
            i_data = sizeof(int);
            for( i_opt = 0; i_opt < i_argc; i_opt++ )
            {
                ppsz_argv[i_opt] = &p_data[i_data + sizeof(int)];
                i_data += *((int *)&p_data[i_data]);
                i_data += sizeof(int);
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
                if( i_opt || config_GetInt( p_this, "playlist-enqueue" ) )
                {
                  playlist_AddExt( p_playlist, ppsz_argv[i_opt],
                    ppsz_argv[i_opt], PLAYLIST_APPEND ,
                    PLAYLIST_END, -1,
                    (char const **)( i_options ? &ppsz_argv[i_opt+1] : NULL ),
                    i_options );
                } else {
                  playlist_AddExt( p_playlist, ppsz_argv[i_opt],
                    ppsz_argv[i_opt], PLAYLIST_APPEND | PLAYLIST_GO,
                    PLAYLIST_END, -1,
                    (char const **)( i_options ? &ppsz_argv[i_opt+1] : NULL ),
                    i_options );
                }

                i_opt += i_options;
            }

            free( ppsz_argv );
        }

        vlc_object_release( p_playlist );
    }

    return DefWindowProc( hwnd, uMsg, wParam, lParam );
}

/*****************************************************************************
 * system_End: terminate winsock.
 *****************************************************************************/
void system_End( vlc_t *p_this )
{
    if( p_this && p_this->p_libvlc && p_this->p_libvlc->psz_vlcpath )
    {
        free( p_this->p_libvlc->psz_vlcpath );
        p_this->p_libvlc->psz_vlcpath = NULL;
    }

    WSACleanup();
}
