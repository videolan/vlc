/**
 * @file platform_android.c
 * @brief Vulkan platform-specific code for Android
 */
/*****************************************************************************
 * Copyright © 2018 Niklas Haas, Thomas Guillem
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
#include <vlc_plugin.h>

#include "instance.h"
#include "../android/utils.h"

static void ClosePlatform(vlc_vk_t *vk);
static int CreateSurface(vlc_vk_t *vk);
static const struct vlc_vk_operations platform_ops =
{
    .close = ClosePlatform,
    .create_surface = CreateSurface,
};

static int InitPlatform(vlc_vk_t *vk)
{
    if (vk->window->type != VOUT_WINDOW_TYPE_ANDROID_NATIVE)
        return VLC_EGENERIC;

    vk->platform_ext = VK_KHR_ANDROID_SURFACE_EXTENSION_NAME;
    vk->platform_ops = &platform_ops;
    return VLC_SUCCESS;
}

static void ClosePlatform(vlc_vk_t *vk)
{
    AWindowHandler_releaseANativeWindow(vk->window->handle.anativewindow,
                                        AWindow_Video);
}

static int CreateSurface(vlc_vk_t *vk, VkInstance vkinst)
{
    ANativeWindow *anw =
        AWindowHandler_getANativeWindow(vk->window->handle.anativewindow,
                                        AWindow_Video);

    VkAndroidSurfaceCreateInfoKHR ainfo = {
         .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
         .pNext = NULL,
         .flags = 0,
         .window = anw,
    };

    VkResult res = vkCreateAndroidSurfaceKHR(vkinst, &ainfo, NULL, &vk->surface);
    if (res != VK_SUCCESS) {
        msg_Err(vk, "Failed creating Android surface");
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

vlc_module_begin()
    set_shortname("Vulkan Android")
    set_description(N_("Android platform support for Vulkan"))
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_capability("vulkan platform", 50)
    set_callback(InitPlatform)
    add_shortcut("vk_android")
vlc_module_end()
