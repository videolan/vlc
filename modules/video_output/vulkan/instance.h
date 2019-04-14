/*****************************************************************************
 * instance.h: Vulkan instance abstraction
 *****************************************************************************
 * Copyright (C) 2018 Niklas Haas
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

#ifndef VLC_VULKAN_INSTANCE_H
#define VLC_VULKAN_INSTANCE_H

#include <vlc_common.h>
#include <vlc_atomic.h>
#include <vlc_vout_window.h>

#include <vulkan/vulkan.h>
#include <libplacebo/vulkan.h>

struct vout_window_t;
struct vout_window_cfg_t;

// Shared struct for vulkan instance / surface / device state
typedef struct vlc_vk_t
{
    // fields internal to instance.c, should not be touched
    struct vlc_object_t obj;
    module_t *module;
    vlc_atomic_rc_t ref_count;
    void *platform_sys;

    // these should be initialized by the surface module (i.e. surface.c)
    struct pl_context *ctx;
    const struct pl_vk_inst *instance;
    const struct pl_vulkan *vulkan;
    const struct pl_swapchain *swapchain;
    VkSurfaceKHR surface;
    struct vout_window_t *window;
} vlc_vk_t;

vlc_vk_t *vlc_vk_Create(struct vout_window_t *, const char *) VLC_USED;
void vlc_vk_Release(vlc_vk_t *);
void vlc_vk_Hold(vlc_vk_t *);

#endif // VLC_VULKAN_INSTANCE_H
