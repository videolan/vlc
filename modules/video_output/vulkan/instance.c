/*****************************************************************************
 * instance.c: Vulkan instance abstraction
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

#include <assert.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_modules.h>
#include <vlc_atomic.h>

#include "instance.h"

/**
 * Creates a Vulkan surface (and its underlying instance).
 *
 * @param wnd window to use as Vulkan surface
 * @param name module name (or NULL for auto)
 * @return a new context, or NULL on failure
 */
vlc_vk_t *vlc_vk_Create(struct vout_window_t *wnd, const char *name)
{
    vlc_object_t *parent = (vlc_object_t *) wnd;
    struct vlc_vk_t *vk;

    vk = vlc_object_create(parent, sizeof (*vk));
    if (unlikely(vk == NULL))
        return NULL;

    vk->ctx = NULL;
    vk->instance = NULL;
    vk->surface = (VkSurfaceKHR) NULL;

    vk->window = wnd;
    vk->module = module_need(vk, "vulkan", name, true);
    if (vk->module == NULL)
    {
        vlc_object_delete(vk);
        return NULL;
    }
    vlc_atomic_rc_init(&vk->ref_count);

    return vk;
}

void vlc_vk_Hold(vlc_vk_t *vk)
{
    vlc_atomic_rc_inc(&vk->ref_count);
}

void vlc_vk_Release(vlc_vk_t *vk)
{
    if (!vlc_atomic_rc_dec(&vk->ref_count))
        return;
    module_unneed(vk, vk->module);
    vlc_object_delete(vk);
}
