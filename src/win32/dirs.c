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
#include "config/configuration.h"

#include <assert.h>
#include <limits.h>

char *config_GetDataDirDefault( void )
{
    return strdup (psz_vlcpath);
}

const char *config_GetLibDir (void)
{
    abort ();
}

const char *config_GetConfDir (void)
{
    static char appdir[PATH_MAX] = "";
    wchar_t wdir[MAX_PATH];

#warning FIXME: thread-safety!
    if (*appdir)
        return appdir;

#if defined (UNDER_CE)
    /*There are some errors in cegcc headers*/
#undef SHGetSpecialFolderPath
    BOOL WINAPI SHGetSpecialFolderPath(HWND,LPWSTR,int,BOOL);
    if( SHGetSpecialFolderPath( NULL, wdir, CSIDL_APPDATA, 1 ) )
#else
    /* Get the "Application Data" folder for all users */
    if( S_OK == SHGetFolderPathW( NULL, CSIDL_COMMON_APPDATA
              | CSIDL_FLAG_CREATE, NULL, SHGFP_TYPE_CURRENT, wdir ) )
#endif
    {
        WideCharToMultiByte (CP_UTF8, 0, wdir, -1,
                             appdir, PATH_MAX, NULL, NULL);
        return appdir;
    }
    return NULL;
}

static char *config_GetShellDir (int csidl)
{
    wchar_t wdir[MAX_PATH];

#if defined (UNDER_CE)
    /*There are some errors in cegcc headers*/
#undef SHGetSpecialFolderPath
    BOOL WINAPI SHGetSpecialFolderPath(HWND,LPWSTR,int,BOOL);
    if (SHGetSpecialFolderPath (NULL, wdir, CSIDL_APPDATA, 1))
#else
    if (SHGetFolderPathW (NULL, csidl | CSIDL_FLAG_CREATE,
                          NULL, SHGFP_TYPE_CURRENT, wdir ) == S_OK)
#endif
        return FromWide (wdir);
    return NULL;
}

static char *config_GetAppDir (void)
{
    char *psz_dir;
    char *psz_parent = config_GetShellDir (CSIDL_APPDATA);

    if (psz_parent == NULL
     ||  asprintf (&psz_dir, "%s\\vlc", psz_parent) == -1)
        psz_dir = NULL;
    free (psz_parent);
    return psz_dir;
}

#warning FIXME Use known folders on Vista and above
char *config_GetUserDir (vlc_userdir_t type)
{
    switch (type)
    {
        case VLC_HOME_DIR:
            return config_GetShellDir (CSIDL_PERSONAL);
        case VLC_CONFIG_DIR:
        case VLC_DATA_DIR:
        case VLC_CACHE_DIR:
            return config_GetAppDir ();

        case VLC_DESKTOP_DIR:
        case VLC_DOWNLOAD_DIR:
        case VLC_TEMPLATES_DIR:
        case VLC_PUBLICSHARE_DIR:
        case VLC_DOCUMENTS_DIR:
            return config_GetUserDir(VLC_HOME_DIR);
        case VLC_MUSIC_DIR:
            return config_GetShellDir (CSIDL_MYMUSIC);
        case VLC_PICTURES_DIR:
            return config_GetShellDir (CSIDL_MYPICTURES);
        case VLC_VIDEOS_DIR:
            return config_GetShellDir (CSIDL_MYVIDEO);
    }
    assert (0);
}
