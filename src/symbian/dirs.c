/*****************************************************************************
 * dirs.cpp: Symbian Directory Structure
 *****************************************************************************
 * Copyright © 2010 Pankaj Yadav
 *
 * Authors: Pankaj Yadav
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

#include "path.h"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include "../libvlc.h"
#include <vlc_charset.h>
#include <vlc_configuration.h>
#include "config/configuration.h"

#include <string.h>

/**
 * Determines the shared data directory
 *
 * @return a null-terminated string or NULL. Use free() to release it.
 */
char *config_GetDataDir (void)
{
    return GetConstPrivatePath();
}

/**
 * Determines the architecture-dependent data directory
 *
 * @return a string (always succeeds).
 */
const char *config_GetLibDir (void)
{
    return "C:\\Sys\\Bin";
}

char *config_GetUserDir (vlc_userdir_t type)
{
    switch (type)
    {
        case VLC_HOME_DIR:
            break;
        case VLC_CONFIG_DIR:
            return GetConstPrivatePath();
        case VLC_DATA_DIR:
            return GetConstPrivatePath();
        case VLC_CACHE_DIR:
            return GetConstPrivatePath();
        case VLC_DESKTOP_DIR:
        case VLC_DOWNLOAD_DIR:
        case VLC_TEMPLATES_DIR:
        case VLC_PUBLICSHARE_DIR:
        case VLC_DOCUMENTS_DIR:
            return strdup("C:\\Data\\Others");
        case VLC_MUSIC_DIR:
            return strdup("C:\\Data\\Sounds");
        case VLC_PICTURES_DIR:
            return strdup("C:\\Data\\Images");
        case VLC_VIDEOS_DIR:
            return strdup("C:\\Data\\Videos");
    }
    return GetConstPrivatePath();
}

