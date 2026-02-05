/*****************************************************************************
 * dirs-common.c: directories configuration
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

#include <vlc_common.h>
#include <vlc_charset.h> // FromWide
#include "../config/configuration.h"

#include <windows.h>
#include <assert.h>


char *config_GetLibDir (void)
{
    /* Get our full path */
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery (config_GetLibDir, &mbi, sizeof(mbi)))
        goto error;

    wchar_t wpath[MAX_PATH];
    if (!GetModuleFileNameW ((HMODULE) mbi.AllocationBase, wpath, MAX_PATH))
        goto error;

    wchar_t *file = wcsrchr (wpath, L'\\');
    if (file == NULL)
        goto error;
    *file = L'\0';

    return FromWide (wpath);
error:
    abort ();
}

static char *config_GetLibexecDir (void)
{
    wchar_t wpath[MAX_PATH];
    if (!GetModuleFileNameW (NULL, wpath, MAX_PATH))
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
            dir = getenv ("VLC_LIB_PATH");
            if (dir)
                return strdup( dir );
            dir = config_GetLibDir();
            break;
        case VLC_PKG_LIBEXEC_DIR:
            dir = getenv ("VLC_LIBEXEC_PATH");
            if (dir)
                return strdup( dir );
            dir = config_GetLibexecDir();
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
    if (unlikely(asprintf(&path, "%s\\%s", dir, filename) == -1))
        path = NULL;
    free(dir);
    return path;
}
