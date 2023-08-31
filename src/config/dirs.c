/*****************************************************************************
 * dirs.c: Platform directories configuration
 *****************************************************************************
 * Copyright (C) 2023 Videolabs
 *
 * Authors: Alexandre Janniaux <ajanni@videolabs.io>
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
# include <config.h>
#endif

#include <vlc_common.h>
#include <vlc_configuration.h>
#include "configuration.h"

static const char *userdir_to_env[] =
{
    [VLC_USERDATA_DIR] = "VLC_USERDATA_PATH",
};

char *config_GetUserDir (vlc_userdir_t type)
{
    if (type >= 0 && type < ARRAY_SIZE(userdir_to_env) &&
        userdir_to_env[type] != NULL)
    {
        const char * path = getenv(userdir_to_env[type]);
        if (path != NULL)
            return strdup(path);
    }
    return platform_GetUserDir(type);
}
