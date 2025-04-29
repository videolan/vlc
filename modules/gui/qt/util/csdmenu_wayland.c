/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#include "csdmenu_wayland.h"

#include <assert.h>
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"

typedef struct {
    struct wl_display* display;
    struct wl_event_queue* queue;
    struct xdg_wm_base *wm_base;
    struct xdg_toplevel *toplevel;
} csd_menu_priv_t;

static void csd_registry_global_cb(void* data, struct wl_registry* registry,
                                   uint32_t id, const char* iface, uint32_t version)
{
    qt_csd_menu_t* obj = (qt_csd_menu_t*)data;
    csd_menu_priv_t* sys = (csd_menu_priv_t*)obj->p_sys;

    if (!strcmp(iface, "xdg_wm_base")) {
        sys->wm_base = (struct xdg_wm_base*)wl_registry_bind(registry, id, &xdg_wm_base_interface, version);
    }
}

static void csd_registry_global_remove_cb(void* data, struct wl_registry* registry, uint32_t id)
{
    VLC_UNUSED(data);
    VLC_UNUSED(registry);
    VLC_UNUSED(id);
}

static const struct wl_registry_listener csd_registry_cbs = {
    csd_registry_global_cb,
    csd_registry_global_remove_cb,
};

static bool CSDMenuPopup(qt_csd_menu_t* p_this, qt_csd_menu_event* event)
{
    csd_menu_priv_t* sys = (csd_menu_priv_t*)p_this->p_sys;
    assert (event->platform == QT_CSD_PLATFORM_WAYLAND);

    if (unlikely(!sys->toplevel))
        return false;

    xdg_toplevel_show_window_menu(sys->toplevel, event->data.wayland.seat, event->data.wayland.serial, event->x, event->y);

    return true;
}

void WaylandCSDMenuClose(vlc_object_t* obj)
{
    qt_csd_menu_t* menu = (qt_csd_menu_t*)obj;
    csd_menu_priv_t* sys = (csd_menu_priv_t*)menu->p_sys;
    if (sys->wm_base)
        xdg_wm_base_destroy(sys->wm_base);
    if (sys->queue)
        wl_event_queue_destroy(sys->queue);
}

int WaylandCSDMenuOpen(qt_csd_menu_t* p_this, qt_csd_menu_info* info)
{
    if (info->platform != QT_CSD_PLATFORM_WAYLAND) {
        return VLC_EGENERIC;
    }

    if (info->data.wayland.display == NULL || info->data.wayland.toplevel == NULL) {
        msg_Warn(p_this, "wayland display or surface not provided");
        return VLC_EGENERIC;
    }

    csd_menu_priv_t* sys = (csd_menu_priv_t*)vlc_obj_calloc(p_this, 1, sizeof(csd_menu_priv_t));
    if (!sys)
        return VLC_ENOMEM;

    p_this->p_sys = sys;

    sys->display = info->data.wayland.display;

    sys->queue = wl_display_create_queue(sys->display);
    void* wrapper = wl_proxy_create_wrapper(sys->display);
    wl_proxy_set_queue((struct wl_proxy*)wrapper, sys->queue);

    struct wl_registry* registry = wl_display_get_registry((struct wl_display*)wrapper);
    wl_proxy_wrapper_destroy(wrapper);

    wl_registry_add_listener(registry, &csd_registry_cbs, p_this);
    wl_display_roundtrip_queue(sys->display, sys->queue);

    wl_registry_destroy(registry);

    if (!sys->wm_base) {
        msg_Dbg(p_this, "wm_base not found");
        goto error;
    }

    sys->toplevel = info->data.wayland.toplevel;

    //module functions
    p_this->popup = CSDMenuPopup;

    return VLC_SUCCESS;

error:
    p_this->p_sys = NULL;
    WaylandCSDMenuClose(VLC_OBJECT(p_this));
    vlc_obj_free(p_this, sys);
    return VLC_EGENERIC;
}

