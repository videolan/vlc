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

#include "platform.h"

int vlc_vk_InitPlatform(vlc_vk_t *vk)
{
    if (vk->window->type != VOUT_WINDOW_TYPE_HWND)
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

void vlc_vk_ClosePlatform(vlc_vk_t *vk)
{
    VLC_UNUSED(vk);
}

const char * const vlc_vk_PlatformExt = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;

int vlc_vk_CreateSurface(vlc_vk_t *vk)
{
    VkInstance vkinst = vk->instance->instance;

    // Get current win32 HINSTANCE
    HINSTANCE hInst = GetModuleHandle(NULL);

    VkWin32SurfaceCreateInfoKHR winfo = {
         .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
         .hinstance = hInst,
         .hwnd = (HWND) vk->window->handle.hwnd,
    };

    VkResult res = vkCreateWin32SurfaceKHR(vkinst, &winfo, NULL, &vk->surface);
    if (res != VK_SUCCESS) {
        msg_Err(vk, "Failed creating Win32 surface");
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}
