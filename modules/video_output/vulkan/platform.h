/*****************************************************************************
 * platform.h: Vulkan platform abstraction
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

#ifndef VLC_VULKAN_PLATFORM_H
#define VLC_VULKAN_PLATFORM_H

#include <vlc_common.h>
#include <vlc_atomic.h>
#include <vlc_vout_window.h>

#include <vulkan/vulkan.h>

struct vout_window_t;
struct vout_window_cfg_t;

struct vlc_vk_platform_t;
struct vlc_vk_platform_operations
{
    void (*close)(struct vlc_vk_platform_t *);
    int (*create_surface)(struct vlc_vk_platform_t *, VkInstance, VkSurfaceKHR *);
};


// Struct for platform-specific Vulkan state
typedef struct vlc_vk_platform_t
{
    // fields internal to platform.c, should not be touched
    struct vlc_object_t obj;
    module_t *module;
    vlc_atomic_rc_t ref_count;
    void *platform_sys;
    const char *platform_ext;

    struct vout_window_t *window;

    const struct vlc_vk_platform_operations *ops;
} vlc_vk_platform_t;

vlc_vk_platform_t *vlc_vk_platform_Create(struct vout_window_t *, const char *) VLC_USED;
void vlc_vk_platform_Release(vlc_vk_platform_t *);
void vlc_vk_platform_Hold(vlc_vk_platform_t *);

// Create a vulkan surface and store it to `surface_out`
static inline int vlc_vk_CreateSurface(vlc_vk_platform_t * vk, VkInstance instance,
                                       VkSurfaceKHR *surface_out)
{
    return vk->ops->create_surface(vk, instance, surface_out);
}

#endif // VLC_VULKAN_PLATFORM_H
