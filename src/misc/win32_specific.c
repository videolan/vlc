/*****************************************************************************
 * win32_specific.c: Win32 specific features
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: win32_specific.c,v 1.25 2003/09/29 17:36:35 gbazin Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@netcourrier.com>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/
#include <string.h>                                              /* strdup() */
#include <stdlib.h>                                                /* free() */

#include <vlc/vlc.h>
#include "vlc_playlist.h"

#ifdef WIN32                       /* optind, getopt(), included in unistd.h */
#   include "../extras/getopt.h"
#endif

#if !defined( UNDER_CE )
#   include <fcntl.h>
#   include <winsock2.h>
#endif

/*****************************************************************************
 * system_Init: initialize winsock and misc other things.
 *****************************************************************************/
void system_Init( vlc_t *p_this, int *pi_argc, char *ppsz_argv[] )
{
#if !defined( UNDER_CE )
    WSADATA Data;

    /* Get our full path */
    if( ppsz_argv[0] )
    {
        char psz_path[MAX_PATH];
        char *psz_vlc;

        GetFullPathName( ppsz_argv[0], MAX_PATH, psz_path, &psz_vlc );

        if( psz_vlc > psz_path && psz_vlc[-1] == '\\' )
        {
            psz_vlc[-1] = '\0';
            p_this->p_libvlc->psz_vlcpath = strdup( psz_path );
        }
        else
        {
            p_this->p_libvlc->psz_vlcpath = strdup( "" );
        }
    }
    else
    {
        p_this->p_libvlc->psz_vlcpath = strdup( "" );
    }

    /* Set the default file-translation mode */
    _fmode = _O_BINARY;
    _setmode( _fileno( stdin ), _O_BINARY ); /* Needed for pipes */

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

#endif
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
    if( config_GetInt( p_this, "high-priority" ) &&
        !SetPriorityClass( GetCurrentProcess(),
                           ABOVE_NORMAL_PRIORITY_CLASS ) )
    {
        if( !SetPriorityClass( GetCurrentProcess(),
                               HIGH_PRIORITY_CLASS ) )
            msg_Dbg( p_this, "can't raise process priority" );
        else
            msg_Dbg( p_this, "raised process priority" );
    }
    else
        msg_Dbg( p_this, "raised process priority" );

    if( config_GetInt( p_this, "one-instance" ) )
    {
        HANDLE hmutex;

        msg_Info( p_this, "One instance mode ENABLED");

        /* Use a named mutex to check if another instance is already running */
        if( ( hmutex = CreateMutex( NULL, TRUE, "VLC ipc "VERSION ) ) == NULL )
        {
            /* Failed for some reason. Just ignore the option and go on as
             * normal. */
            msg_Err( p_this, "One instance mode DISABLED "
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
                msg_Err( p_this, "One instance mode DISABLED "
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
            if( ( ipcwindow = FindWindow( NULL, "VLC ipc "VERSION ) )
                == NULL )
            {
                msg_Err( p_this, "One instance mode DISABLED "
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
        CreateWindow( "STATIC",                      /* name of window class */
                  "VLC ipc "VERSION,                /* window title bar text */
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

                playlist_Add( p_playlist, ppsz_argv[ i_opt ],
                    (char const **)( i_options ? &ppsz_argv[i_opt+1] : NULL ),
                    i_options, PLAYLIST_APPEND | (i_opt? 0 : PLAYLIST_GO),
                    PLAYLIST_END );

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
#if !defined( UNDER_CE )
    WSACleanup();
#endif
}
