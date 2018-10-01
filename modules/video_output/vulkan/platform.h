/*****************************************************************************
 * platform.h: Vulkan platform-specific functions
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

#include "instance.h"

// Initializes a platform-specific context to vk->platform_sys
int vlc_vk_InitPlatform(vlc_vk_t *);
void vlc_vk_ClosePlatform(vlc_vk_t *);

// Contains the required platform-specific instance extension
extern const char * const vlc_vk_PlatformExt;

// Create a vulkan surface to vk->surface
int vlc_vk_CreateSurface(vlc_vk_t *);

#endif // VLC_VULKAN_PLATFORM_H
