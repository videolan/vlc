/*****************************************************************************
 * plugin.c : Low-level dynamic library handling
 *****************************************************************************
 * Copyright (C) 2001-2011 VLC authors and VideoLAN
 *
 * Authors: Sam Hocevar <sam@zoy.org>
 *          Ethan C. Baldridge <BaldridgeE@cadmus.com>
 *          Hans-Peter Jansen <hpj@urpla.net>
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

#include <vlc_common.h>
#include <vlc_charset.h>
#include "modules/modules.h"
#include <windows.h>
#include <wchar.h>

extern DWORD LoadLibraryFlags;

#if (_WIN32_WINNT < _WIN32_WINNT_WIN7)
static BOOL WINAPI SetThreadErrorModeFallback(DWORD mode, DWORD *oldmode)
{
    /* TODO: cache the pointer */
    HANDLE h = GetModuleHandle(_T("kernel32.dll"));
    if (unlikely(h == NULL))
        return FALSE;

    BOOL (WINAPI *SetThreadErrorModeReal)(DWORD, DWORD *);

    SetThreadErrorModeReal = GetProcAddress(h, "SetThreadErrorMode");
    if (SetThreadErrorModeReal != NULL)
        return SetThreadErrorModeReal(mode, oldmode);

# if (_WIN32_WINNT < _WIN32_WINNT_VISTA)
    /* As per libvlc_new() documentation, the calling process is responsible
     * for setting a proper error mode on Windows 2008 and earlier versions.
     * This is only a sanity check. */
    UINT (WINAPI *GetErrorModeReal)(void);
    DWORD curmode = 0;

    GetErrorModeReal = (void *)GetProcAddress(h, "GetErrorMode");
    if (GetErrorModeReal != NULL)
        curmode = GetErrorModeReal();
    else
        curmode = SEM_FAILCRITICALERRORS;
# else
    DWORD curmode = GetErrorMode();
# endif
    /* Extra flags should be OK. Missing flags are NOT OK. */
    if ((mode & curmode) != mode)
        return FALSE;
    if (oldmode != NULL)
        *oldmode = curmode;
    return TRUE;
}
# define SetThreadErrorMode SetThreadErrorModeFallback
#endif

static char *GetWindowsError( void )
{
    wchar_t wmsg[256];
    int i = 0, i_error = GetLastError();

    FormatMessageW( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL, i_error, MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
                    wmsg, 256, NULL );

    /* Go to the end of the string */
    while( !wmemchr( L"\r\n\0", wmsg[i], 3 ) )
        i++;

    snwprintf( wmsg + i, 256 - i, L" (error %i)", i_error );
    return FromWide( wmsg );
}

int module_Load( vlc_object_t *p_this, const char *psz_file,
                 module_handle_t *p_handle, bool lazy )
{
    wchar_t *wfile = ToWide (psz_file);
    if (wfile == NULL)
        return -1;

    module_handle_t handle = NULL;
#if !VLC_WINSTORE_APP
    DWORD mode;
    if (SetThreadErrorMode (SEM_FAILCRITICALERRORS, &mode) != 0)
    {
        handle = LoadLibraryExW (wfile, NULL, LoadLibraryFlags );
        SetThreadErrorMode (mode, NULL);
    }
#else
    handle = LoadPackagedLibrary( wfile, 0 );
#endif
    free (wfile);

    if( handle == NULL )
    {
        char *psz_err = GetWindowsError();
        msg_Warn( p_this, "cannot load module `%s' (%s)", psz_file, psz_err );
        free( psz_err );
        return -1;
    }

    *p_handle = handle;
    (void) lazy;
    return 0;
}

void module_Unload( module_handle_t handle )
{
    FreeLibrary( handle );
}

void *module_Lookup( module_handle_t handle, const char *psz_function )
{
    return (void *)GetProcAddress( handle, (char *)psz_function );
}
