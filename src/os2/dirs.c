/*****************************************************************************
 * dirs.c: OS/2 directories configuration
 *****************************************************************************
 * Copyright (C) 2010 the VideoLAN team
 *
 * Authors: KO Myung-Hun <komh@chollian.net>
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

#include "../libvlc.h"
#include <vlc_charset.h>
#include "config/configuration.h"

static char *config_GetVlcDir (void)
{
    return FromLocaleDup (psz_vlcpath);
}

/**
 * Determines the shared data directory
 *
 * @return a nul-terminated string or NULL. Use free() to release it.
 */
char *config_GetDataDirDefault (void)
{
    char *datadir = config_GetVlcDir();

    if (datadir)
        /* replace last lib\vlc with share */
        strcpy ( datadir + strlen (datadir) - 7, "share");

    return datadir;
}

/**
 * Determines the architecture-dependent data directory
 *
 * @return a string (always succeeds).
 */
const char *config_GetLibDir (void)
{
    abort ();
}

/**
 * Determines the system configuration directory.
 *
 * @return a string (always succeeds).
 */
const char *config_GetConfDir( void )
{
    return config_GetVlcDir ();
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
    return config_GetVlcDir ();
}
