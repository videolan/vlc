/*****************************************************************************
 * dirs.c: Android directories configuration
 *****************************************************************************
 * Copyright © 2012 Rafaël Carré
 *
 * Authors: Rafaël Carré <funman@videolanorg>
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

#include "config/configuration.h"

#include <string.h>

char *config_GetUserDir (vlc_userdir_t type)
{
    switch (type)
    {
        case VLC_DATA_DIR:
            return strdup("/sdcard/Android/data/org.videolan.vlc");
        case VLC_CACHE_DIR:
            return strdup("/sdcard/Android/data/org.videolan.vlc/cache");

        case VLC_HOME_DIR:
        case VLC_CONFIG_DIR:
            return NULL;

        case VLC_DESKTOP_DIR:
        case VLC_DOWNLOAD_DIR:
        case VLC_TEMPLATES_DIR:
        case VLC_PUBLICSHARE_DIR:
        case VLC_DOCUMENTS_DIR:
        case VLC_MUSIC_DIR:
        case VLC_PICTURES_DIR:
        case VLC_VIDEOS_DIR:
            return NULL;
    }
    return NULL;
}
