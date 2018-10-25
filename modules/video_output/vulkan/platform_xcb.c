/**
 * @file platform_xcb.c
 * @brief Vulkan platform-specific code for X11/xcb
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

#include "platform.h"

int vlc_vk_InitPlatform(vlc_vk_t *vk)
{
    if (vk->window->type != VOUT_WINDOW_TYPE_XID)
        return VLC_EGENERIC;

    const char *display = vk->window->display.x11;
    xcb_connection_t *conn = vk->platform_sys = xcb_connect(display, NULL);
    if (xcb_connection_has_error(conn))
    {
        msg_Err(vk, "Failed connecting to X server (%s)",
                display ? display : "default");
        xcb_disconnect(conn);
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

void vlc_vk_ClosePlatform(vlc_vk_t *vk)
{
    xcb_connection_t *conn = vk->platform_sys;

    xcb_disconnect(conn);
}

const char * const vlc_vk_PlatformExt = VK_KHR_XCB_SURFACE_EXTENSION_NAME;

int vlc_vk_CreateSurface(vlc_vk_t *vk)
{
    VkInstance vkinst = vk->instance->instance;
    xcb_connection_t *conn = vk->platform_sys;

    VkXcbSurfaceCreateInfoKHR xinfo = {
         .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
         .window = (xcb_window_t) vk->window->handle.xid,
         .connection = conn,
    };

    VkResult res = vkCreateXcbSurfaceKHR(vkinst, &xinfo, NULL, &vk->surface);
    if (res != VK_SUCCESS) {
        msg_Err(vk, "Failed creating XCB surface");
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}
