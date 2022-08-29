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
#include <vulkan/vulkan.h>

typedef struct vlc_vk_instance_t
{
    VkInstance instance;
    PFN_vkGetInstanceProcAddr get_proc_address;
} vlc_vk_instance_t;

#endif // VLC_VULKAN_INSTANCE_H
