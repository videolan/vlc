/*****************************************************************************
 * dirs.c: directories configuration
 *****************************************************************************
 * Copyright (C) 2001-2010 VLC authors and VideoLAN
 * Copyright © 2007-2012 Rémi Denis-Courmont
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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
#define UNICODE
#endif
#include <vlc_common.h>

#ifdef __MINGW32__
# include <w32api.h>
#endif
#include <direct.h>
#include <shlobj.h>

#include "../libvlc.h"
#include <vlc_charset.h>
#include <vlc_configuration.h>
#include "config/configuration.h"

#include <assert.h>
#include <limits.h>


char *config_GetLibDir (void)
{
    /* Get our full path */
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery (config_GetLibDir, &mbi, sizeof(mbi)))
        goto error;

    wchar_t wpath[MAX_PATH];
    if (!GetModuleFileName ((HMODULE) mbi.AllocationBase, wpath, MAX_PATH))
        goto error;

    wchar_t *file = wcsrchr (wpath, L'\\');
    if (file == NULL)
        goto error;
    *file = L'\0';

    return FromWide (wpath);
error:
    abort ();
}

static char *config_GetDataDir(void)
{
    const char *path = getenv ("VLC_DATA_PATH");
    return (path != NULL) ? strdup (path) : config_GetLibDir ();
}

char *config_GetSysPath(vlc_sysdir_t type, const char *filename)
{
    char *dir = NULL;

    switch (type)
    {
        case VLC_PKG_DATA_DIR:
            dir = config_GetDataDir();
            break;
        case VLC_PKG_LIB_DIR:
        case VLC_PKG_LIBEXEC_DIR:
            dir = config_GetLibDir();
            break;
        case VLC_SYSDATA_DIR:
            break;
        case VLC_LOCALE_DIR:
            dir = config_GetSysPath(VLC_PKG_DATA_DIR, "locale");
            break;
        default:
            vlc_assert_unreachable();
    }

    if (filename == NULL || unlikely(dir == NULL))
        return dir;

    char *path;
    if (unlikely(asprintf(&path, "%s/%s", dir, filename) == -1))
        path = NULL;
    free(dir);
    return path;
}

static char *config_GetShellDir (int csidl)
{
    wchar_t wdir[MAX_PATH];

    if (SHGetFolderPathW (NULL, csidl | CSIDL_FLAG_CREATE,
                          NULL, SHGFP_TYPE_CURRENT, wdir ) == S_OK)
        return FromWide (wdir);
    return NULL;
}

static char *config_GetAppDir (void)
{
    /* if portable directory exists, use it */
    WCHAR path[MAX_PATH];
    if (GetModuleFileName (NULL, path, MAX_PATH))
    {
        WCHAR *lastDir = wcsrchr (path, TEXT('\\'));
        if (lastDir)
        {
            wcscpy (lastDir + 1, TEXT("portable"));
            DWORD attrib = GetFileAttributes (path);
            if (attrib != INVALID_FILE_ATTRIBUTES &&
                    (attrib & FILE_ATTRIBUTE_DIRECTORY))
                return FromWide (path);
        }
    }

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
        case VLC_USERDATA_DIR:
            return config_GetAppDir ();
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
    vlc_assert_unreachable ();
}
