/**
 * @file surface.c
 * @brief Vulkan platform-specific surface extension module
 */
/*****************************************************************************
 * Copyright © 2018 Niklas Haas
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

#include <stdlib.h>
#include <assert.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_window.h>

#include "../placebo_utils.h"
#include "instance.h"
#include "platform.h"

static const struct vlc_vk_operations platform_ops =
{
    .create_surface = vlc_vk_CreateSurface,
    .close = vlc_vk_ClosePlatform,
};

static int Open (vlc_object_t *obj)
{
    vlc_vk_t *vk = (vlc_vk_t *) obj;

    if (vlc_vk_InitPlatform(vk) != VLC_SUCCESS)
        goto error;

    vk->platform_ext = vlc_vk_PlatformExt;
    vk->ops = &platform_ops;
    return VLC_SUCCESS;

error:

    vlc_vk_ClosePlatform(vk);
    return VLC_EGENERIC;
}

#define XSTR(x) #x
#define STR(x) XSTR(x)

vlc_module_begin ()
    set_shortname ("Vulkan/" STR(PLATFORM_NAME))
    set_description ("Vulkan context (" STR(PLATFORM_NAME) ")")
    set_category (CAT_VIDEO)
    set_subcategory (SUBCAT_VIDEO_VOUT)
    set_capability ("vulkan", 10)
    set_callback (Open)
vlc_module_end ()
