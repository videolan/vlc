/*****************************************************************************
 * platform.c: Vulkan platform abstraction
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

#include "platform.h"

static int vlc_vk_start(void *func, bool forced, va_list ap)
{
    int (*activate)(vlc_vk_platform_t *vk) = func;
    vlc_vk_platform_t *vk = va_arg(ap, vlc_vk_platform_t *);

    int ret = activate(vk);
    /* TODO: vlc_objres_clear, which is not in the public API. */
    (void)forced;
    return ret;
}

/**
 * Initializes a Vulkan platform module for a given window
 *
 * @param wnd window to use as Vulkan surface
 * @param name module name (or NULL for auto)
 * @return a new platform object, or NULL on failure
 */
vlc_vk_platform_t *vlc_vk_platform_Create(struct vout_window_t *wnd, const char *name)
{
    vlc_object_t *parent = (vlc_object_t *) wnd;
    struct vlc_vk_platform_t *vk;

    vk = vlc_object_create(parent, sizeof (*vk));
    if (unlikely(vk == NULL))
        return NULL;

    vk->platform_ext = NULL;
    vk->ops = NULL;
    vk->window = wnd;

    vk->module = vlc_module_load(wnd, "vulkan platform", name, false,
                                 vlc_vk_start, vk);

    if (vk->module == NULL)
    {
        vlc_object_delete(vk);
        return NULL;
    }

    return vk;
}

void vlc_vk_platform_Release(vlc_vk_platform_t *vk)
{
    if (vk->ops)
        vk->ops->close(vk);

    /* TODO: use vlc_objres_clear */
    vlc_object_delete(vk);
}
