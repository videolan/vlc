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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>

#include "platform.h"

static void ClosePlatform(vlc_vk_platform_t *vk);
static int CreateSurface(vlc_vk_platform_t *vk, const vlc_vk_instance_t *, VkSurfaceKHR *);

static const struct vlc_vk_platform_operations platform_ops =
{
    .close = ClosePlatform,
    .create_surface = CreateSurface,
};

static int InitPlatform(vlc_vk_platform_t *vk)
{
    if (vk->window->type != VLC_WINDOW_TYPE_XID)
        return VLC_EGENERIC;

    const char *display = vk->window->display.x11;
    xcb_connection_t *conn = xcb_connect(display, NULL);
    if (xcb_connection_has_error(conn))
    {
        msg_Err(vk, "Failed connecting to X server (%s)",
                display ? display : "default");
        xcb_disconnect(conn);
        return VLC_EGENERIC;
    }

    vk->platform_sys = conn;
    vk->platform_ext = VK_KHR_XCB_SURFACE_EXTENSION_NAME;
    vk->ops = &platform_ops;

    return VLC_SUCCESS;
}

static void ClosePlatform(vlc_vk_platform_t *vk)
{
    xcb_connection_t *conn = vk->platform_sys;
    xcb_disconnect(conn);
}

static int CreateSurface(vlc_vk_platform_t *vk, const vlc_vk_instance_t *inst,
                         VkSurfaceKHR *surface_out)
{
    xcb_connection_t *conn = vk->platform_sys;

    VkXcbSurfaceCreateInfoKHR xinfo = {
         .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
         .window = (xcb_window_t) vk->window->handle.xid,
         .connection = conn,
    };

    VkResult res = vkCreateXcbSurfaceKHR(inst->instance, &xinfo, NULL, surface_out);
    if (res != VK_SUCCESS) {
        msg_Err(vk, "Failed creating XCB surface");
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

vlc_module_begin()
    set_shortname("Vulkan XCB")
    set_description(N_("XCB/X11 platform support for Vulkan"))
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_capability("vulkan platform", 50)
    set_callback(InitPlatform)
    add_shortcut("vk_x11")
vlc_module_end()
