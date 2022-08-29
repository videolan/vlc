/**
 * @file platform_win32.c
 * @brief Vulkan platform-specific code for Win32
 */
/*****************************************************************************
 * Copyright © 2018 Niklas Haas, Marvin Scholz
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
#include <vlc_plugin.h>
#include "platform.h"

static void ClosePlatform(vlc_vk_platform_t *vk)
{
    VLC_UNUSED(vk);
}

static int CreateSurface(vlc_vk_platform_t *vk, const vlc_vk_instance_t *inst,
                         VkSurfaceKHR *surface_out)
{
    // Get current win32 HINSTANCE
    HINSTANCE hInst = GetModuleHandle(NULL);

    VkWin32SurfaceCreateInfoKHR winfo = {
         .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
         .hinstance = hInst,
         .hwnd = (HWND) vk->window->handle.hwnd,
    };

    VkResult res = vkCreateWin32SurfaceKHR(inst->instance, &winfo, NULL, surface_out);
    if (res != VK_SUCCESS) {
        msg_Err(vk, "Failed creating Win32 surface");
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static const struct vlc_vk_platform_operations platform_ops =
{
    .close = ClosePlatform,
    .create_surface = CreateSurface,
};

static int InitPlatform(vlc_vk_platform_t *vk)
{
    if (vk->window->type != VLC_WINDOW_TYPE_HWND)
        return VLC_EGENERIC;

    vk->platform_ext = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
    vk->ops = &platform_ops;
    return VLC_SUCCESS;
}

vlc_module_begin()
    set_shortname("Vulkan Win32")
    set_description(N_("Win32 platform support for Vulkan"))
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_capability("vulkan platform", 50)
    set_callback(InitPlatform)
    add_shortcut("vk_win32")
vlc_module_end()
