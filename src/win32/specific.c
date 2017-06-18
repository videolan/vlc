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

#ifndef UNICODE
# define UNICODE
#endif
#include <vlc_common.h>
#include "libvlc.h"
#include "../lib/libvlc_internal.h"
#include "config/vlc_getopt.h"

#include <mmsystem.h>
#include <winsock.h>
#if VLC_WINSTORE_APP && !defined(__MINGW32__)
typedef UINT MMRESULT;
#endif

DWORD LoadLibraryFlags = 0;

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
    if (system_InitWSA(2, 2) && system_InitWSA(1, 1))
        fputs("Error: cannot initialize Winsocks\n", stderr);

#if !VLC_WINSTORE_APP
# if (_WIN32_WINNT < _WIN32_WINNT_WIN8)
    if (GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")),
                                       "SetDefaultDllDirectories") != NULL)
# endif /* FIXME: not reentrant */
        LoadLibraryFlags = LOAD_LIBRARY_SEARCH_SYSTEM32;
#endif
}

/*****************************************************************************
 * system_Configure: check for system specific configuration options.
 *****************************************************************************/

/* Must be same as in modules/control/win_msg.c */
typedef struct
{
    int argc;
    int enqueue;
    char data[];
} vlc_ipc_data_t;

void system_Configure( libvlc_int_t *p_this, int i_argc, const char *const ppsz_argv[] )
{
#if !VLC_WINSTORE_APP
    if( var_InheritBool( p_this, "one-instance" )
     || ( var_InheritBool( p_this, "one-instance-when-started-from-file" )
       && var_InheritBool( p_this, "started-from-file" ) ) )
    {
        HANDLE hmutex;

        msg_Info( p_this, "one instance mode ENABLED");

        /* Use a named mutex to check if another instance is already running */
        if( !( hmutex = CreateMutex( 0, TRUE, L"VLC ipc " TEXT(VERSION) ) ) )
        {
            /* Failed for some reason. Just ignore the option and go on as
             * normal. */
            msg_Err( p_this, "one instance mode DISABLED "
                     "(mutex couldn't be created)" );
            return;
        }

        if( GetLastError() != ERROR_ALREADY_EXISTS )
        {
            libvlc_InternalAddIntf( p_this, "win_msg,none" );
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
            if( !( ipcwindow = FindWindow( 0, L"VLC ipc " TEXT(VERSION) ) ) )
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

/**
 * Cleans up after system_Init() and system_Configure().
 */
void system_End(void)
{
    /* XXX: In theory, we should not call this if WSAStartup() failed. */
    WSACleanup();
}
