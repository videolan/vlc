/*****************************************************************************
 * dirs.c: directories configuration
 *****************************************************************************
 * Copyright (C) 2001-2007 the VideoLAN team
 * Copyright © 2007-2008 Rémi Denis-Courmont
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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

#include <vlc_common.h>

#ifndef _WIN32_IE
# define _WIN32_IE 0x0501
#endif
#include <w32api.h>
#ifndef UNDER_CE
# include <direct.h>
#endif
#include <shlobj.h>

#include "../libvlc.h"
#include <vlc_charset.h>
#include <vlc_configuration.h>

#include <assert.h>
#include <limits.h>

const char *config_GetDataDir( void )
{
    static char path[PATH_MAX] = "";
#warning FIXME: thread-safety!

    if( *path == '\0' )
        strlcpy (path, psz_vlcpath, sizeof (path));
    return path;
}

static const char *GetDir( bool b_common )
{
    wchar_t wdir[MAX_PATH];

#if defined (UNDER_CE)
    /*There are some errors in cegcc headers*/
#undef SHGetSpecialFolderPath
    BOOL WINAPI SHGetSpecialFolderPath(HWND,LPWSTR,int,BOOL);
    if( SHGetSpecialFolderPath( NULL, wdir, CSIDL_APPDATA, 1 ) )
#else
    /* Get the "Application Data" folder for the current user */
    if( S_OK == SHGetFolderPathW( NULL, (b_common ? CSIDL_COMMON_APPDATA
                                                  : CSIDL_APPDATA)
              | CSIDL_FLAG_CREATE, NULL, SHGFP_TYPE_CURRENT, wdir ) )
#endif
    {
        static char appdir[PATH_MAX] = "";
        static char comappdir[PATH_MAX] = "";
        WideCharToMultiByte (CP_UTF8, 0, wdir, -1,
                             b_common ? comappdir : appdir,
                             PATH_MAX, NULL, NULL);
        return b_common ? comappdir : appdir;
    }
    return NULL;
}

const char *config_GetConfDir( void )
{
    return GetDir( true );
}

static char *config_GetHomeDir (void)
{
    wchar_t wdir[MAX_PATH];

#if defined (UNDER_CE)
    /*There are some errors in cegcc headers*/
#undef SHGetSpecialFolderPath
    BOOL WINAPI SHGetSpecialFolderPath(HWND,LPWSTR,int,BOOL);
    if (SHGetSpecialFolderPath (NULL, wdir, CSIDL_APPDATA, 1))
#else
    if (SHGetFolderPathW (NULL, CSIDL_PERSONAL | CSIDL_FLAG_CREATE,
                          NULL, SHGFP_TYPE_CURRENT, wdir ) == S_OK)
#endif
        return FromWide (wdir);
    return NULL;
}

static char *config_GetAppDir (void)
{
    char *psz_dir;
    const char *psz_parent = GetDir (false);

    if( asprintf( &psz_dir, "%s\\vlc", psz_parent ) == -1 )
        psz_dir = NULL;
    return psz_dir;
}

char *config_GetCacheDir( void )
{
    return config_GetAppDir ();
}

char *config_GetUserDir (vlc_userdir_t type)
{
    switch (type)
    {
        case VLC_HOME_DIR:
            return config_GetHomeDir ();
        case VLC_CONFIG_DIR:
            return config_GetAppDir ();
        case VLC_DATA_DIR:
            return config_GetAppDir ();
        case VLC_DESKTOP_DIR:
        case VLC_DOWNLOAD_DIR:
        case VLC_TEMPLATES_DIR:
        case VLC_PUBLICSHARE_DIR:
        case VLC_DOCUMENTS_DIR:
        case VLC_MUSIC_DIR:
        case VLC_PICTURES_DIR:
        case VLC_VIDEOS_DIR:
            return config_GetUserDir (VLC_HOME_DIR);
    }
    assert (0);
}
