/*****************************************************************************
 * dirs.c: OS/2 directories configuration
 *****************************************************************************
 * Copyright (C) 2010 VLC authors and VideoLAN
 *
 * Authors: KO Myung-Hun <komh@chollian.net>
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

#include "../libvlc.h"
#include <vlc_charset.h>
#include "config/configuration.h"

char *config_GetLibDir (void)
{
    HMODULE hmod;
    CHAR    psz_path[CCHMAXPATH + 4];

    DosQueryModFromEIP( &hmod, NULL, 0, NULL, NULL, ( ULONG )system_Init );
    DosQueryModuleName( hmod, sizeof( psz_path ), psz_path );

    /* remove the DLL name */
    char *slash = strrchr( psz_path, '\\');
    if( slash == NULL )
        abort();
    strcpy(slash + 1, PACKAGE);
    return FromLocaleDup(psz_path);
}

/**
 * Determines the shared data directory
 *
 * @return a nul-terminated string or NULL. Use free() to release it.
 */
char *config_GetDataDir (void)
{
    const char *path = getenv ("VLC_DATA_PATH");
    if (path)
        return strdup (path);

    char *datadir = config_GetLibDir();
    if (datadir)
        /* replace last lib\vlc with share */
        strcpy ( datadir + strlen (datadir) - 7, "share");
    return datadir;
}

static char *config_GetHomeDir (void)
{
    const char *home = getenv ("HOME");
    if (home != NULL)
        return FromLocaleDup (home);

    return config_GetLibDir();
}

char *config_GetUserDir (vlc_userdir_t type)
{
    switch (type)
    {
        case VLC_HOME_DIR:
        case VLC_CONFIG_DIR:
        case VLC_DATA_DIR:
        case VLC_CACHE_DIR:
        case VLC_DESKTOP_DIR:
        case VLC_DOWNLOAD_DIR:
        case VLC_TEMPLATES_DIR:
        case VLC_PUBLICSHARE_DIR:
        case VLC_DOCUMENTS_DIR:
        case VLC_MUSIC_DIR:
        case VLC_PICTURES_DIR:
        case VLC_VIDEOS_DIR:
            break;
    }
    return config_GetHomeDir ();
}
