/**
 * @file idle-inhibit.c
 * @brief Wayland idle inhibitor module for VLC media player
 */
/*****************************************************************************
 * Copyright © 2018 Rémi Denis-Courmont
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

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <wayland-client.h>
#include "idle-inhibit-client-protocol.h"

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_window.h>
#include <vlc_inhibit.h>

struct vlc_inhibit_sys
{
    struct wl_event_queue *eventq;
    struct zwp_idle_inhibit_manager_v1 *manager;
    struct zwp_idle_inhibitor_v1 *inhibitor;
};

static void Inhibit (vlc_inhibit_t *ih, unsigned mask)
{
    vout_window_t *wnd = vlc_inhibit_GetWindow(ih);
    vlc_inhibit_sys_t *sys = ih->p_sys;
    bool suspend = (mask & VLC_INHIBIT_DISPLAY) != 0;

    if (sys->inhibitor != NULL) {
        zwp_idle_inhibitor_v1_destroy(sys->inhibitor);
        sys->inhibitor = NULL;
    }

    if (suspend)
        sys->inhibitor = zwp_idle_inhibit_manager_v1_create_inhibitor(
            sys->manager, wnd->handle.wl);

}

static void registry_global_cb(void *data, struct wl_registry *registry,
                               uint32_t name, const char *iface, uint32_t vers)
{
    vlc_inhibit_t *ih = data;
    vlc_inhibit_sys_t *sys = ih->p_sys;

    if (!strcmp(iface, "zwp_idle_inhibit_manager_v1"))
        sys->manager = wl_registry_bind(registry, name,
                                    &zwp_idle_inhibit_manager_v1_interface, 1);
    (void) vers;
}

static void registry_global_remove_cb(void *data, struct wl_registry *registry,
                                      uint32_t name)
{
    (void) data; (void) registry; (void) name;
}

static const struct wl_registry_listener registry_cbs =
{
    registry_global_cb,
    registry_global_remove_cb,
};

static int Open(vlc_object_t *obj)
{
    vlc_inhibit_t *ih = (vlc_inhibit_t *)obj;
    vout_window_t *wnd = vlc_inhibit_GetWindow(ih);

    if (wnd->type != VOUT_WINDOW_TYPE_WAYLAND)
        return VLC_EGENERIC;

    vlc_inhibit_sys_t *sys = vlc_obj_malloc(obj, sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->manager = NULL;
    sys->inhibitor = NULL;
    ih->p_sys = sys;

    struct wl_display *display = wnd->display.wl;

    sys->eventq = wl_display_create_queue(display);
    if (sys->eventq == NULL)
        return VLC_ENOMEM;

    struct wl_registry *registry = wl_display_get_registry(display);
    if (registry == NULL)
        goto error;

    wl_registry_add_listener(registry, &registry_cbs, ih);
    wl_proxy_set_queue((struct wl_proxy *)registry, sys->eventq);
    wl_display_roundtrip_queue(display, sys->eventq);
    wl_registry_destroy(registry);

    if (sys->manager == NULL)
        goto error;

    ih->inhibit = Inhibit;
    return VLC_SUCCESS;

error:
    if (sys->eventq != NULL)
        wl_event_queue_destroy(sys->eventq);
    return VLC_EGENERIC;
}

static void Close(vlc_object_t *obj)
{
    vlc_inhibit_t *ih = (vlc_inhibit_t *)obj;
    vlc_inhibit_sys_t *sys = ih->p_sys;
    vout_window_t *wnd = vlc_inhibit_GetWindow(ih);

    if (sys->inhibitor != NULL)
        zwp_idle_inhibitor_v1_destroy(sys->inhibitor);

    zwp_idle_inhibit_manager_v1_destroy(sys->manager);
    wl_display_flush(wnd->display.wl);
    wl_event_queue_destroy(sys->eventq);
}

vlc_module_begin()
    set_shortname(N_("WL idle"))
    set_description(N_("Wayland idle inhibitor"))
    set_category(CAT_ADVANCED)
    set_subcategory(SUBCAT_ADVANCED_MISC)
    set_capability("inhibit", 30)
    set_callbacks(Open, Close)
vlc_module_end()
