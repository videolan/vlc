/**
 * @file registry.c
 * @brief Wayland client register common helpers for VLC media player
 */
/*****************************************************************************
 * Copyright © 2019 Rémi Denis-Courmont
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
#include <search.h>
#include <stdlib.h>
#include <string.h>

#include <wayland-client.h>
#include <vlc_common.h>
#include "registry.h"

struct vlc_wl_interface;

struct vlc_wl_global
{
    uint32_t name;
    uint32_t version;
    struct vlc_wl_interface *interface;
    struct wl_list node;
};

struct vlc_wl_interface
{
    struct wl_list globals;
    char interface[];
};

static struct vlc_wl_global *vlc_wl_global_add(struct vlc_wl_interface *vi,
                                               uint32_t name, uint32_t version)
{
    struct vlc_wl_global *vg = malloc(sizeof (*vg));
    if (likely(vg != NULL))
    {
        vg->name = name;
        vg->version = version;
        vg->interface = vi;
        wl_list_insert(&vi->globals, &vg->node);
    }
    return vg;
}

static void vlc_wl_global_remove(struct vlc_wl_global *vg)
{
    wl_list_remove(&vg->node);
    free(vg);
}

static struct vlc_wl_interface *vlc_wl_interface_create(const char *iface)
{
    const size_t len = strlen(iface) + 1;
    struct vlc_wl_interface *vi = malloc(sizeof (*vi) + len);

    if (likely(vi != NULL))
    {
        memcpy(vi->interface, iface, len);
        wl_list_init(&vi->globals);
    }
    return vi;
}

static void vlc_wl_interface_destroy(struct vlc_wl_interface *vi)
{
    assert(wl_list_empty(&vi->globals));
    free(vi);
}

struct vlc_wl_registry
{
    struct wl_registry *registry;
    void *interfaces;
    void *names;
};

static int vwimatch(const void *a, const void *b)
{
    const char *name = a;
    const struct vlc_wl_interface *iface = b;

    return strcmp(name, iface->interface);
}

static struct vlc_wl_interface *
vlc_wl_interface_find(const struct vlc_wl_registry *vr, const char *interface)
{
    void **pvi = tfind(interface, &vr->interfaces, vwimatch);

    return (pvi != NULL) ? *pvi : NULL;
}

static struct vlc_wl_global *
vlc_wl_global_find(const struct vlc_wl_registry *vr, const char *interface)
{
    struct vlc_wl_interface *vi = vlc_wl_interface_find(vr, interface);

    if (vi == NULL || wl_list_empty(&vi->globals))
        return 0;

    return container_of(vi->globals.next, struct vlc_wl_global, node);
}

uint_fast32_t vlc_wl_interface_get_version(struct vlc_wl_registry *vr,
                                           const char *interface)
{
    const struct vlc_wl_global *vg = vlc_wl_global_find(vr, interface);

    return (vg != NULL) ? vg->version : 0;
}

struct wl_proxy *vlc_wl_interface_bind(struct vlc_wl_registry *vr,
                                       const char *interface,
                                       const struct wl_interface *wi,
                                       uint32_t *restrict version)
{
    const struct vlc_wl_global *vg = vlc_wl_global_find(vr, interface);

    if (vg == NULL)
        return 0;

    uint32_t vers = (version != NULL) ? *version : 1;

    if (vers > vg->version)
        *version = vers = vg->version;

    return wl_registry_bind(vr->registry, vg->name, wi, vers);
}

static int vwicmp(const void *a, const void *b)
{
    const struct vlc_wl_interface *ia = a;
    const struct vlc_wl_interface *ib = b;

    return strcmp(ia->interface, ib->interface);
}

static int vwncmp(const void *a, const void *b)
{
    const struct vlc_wl_global *ga = a;
    const struct vlc_wl_global *gb = b;

    return (ga->name < gb->name) ? -1 : (ga->name > gb->name);
}

static void registry_global_cb(void *data, struct wl_registry *registry,
                               uint32_t name, const char *iface, uint32_t vers)
{
    struct vlc_wl_registry *vr = data;
    struct vlc_wl_interface *vi = vlc_wl_interface_create(iface);

    void **pvi = tsearch(vi, &vr->interfaces, vwicmp);
    if (unlikely(pvi == NULL))
    {
        vlc_wl_interface_destroy(vi);
        return;
    }

    if (*pvi != vi)
    {
        vlc_wl_interface_destroy(vi);
        vi = *pvi;
    }

    struct vlc_wl_global *vg = vlc_wl_global_add(vi, name, vers);
    if (unlikely(vg == NULL))
        return;

    void **pvg = tsearch(vg, &vr->names, vwncmp);
    if (unlikely(pvg == NULL) /* OOM */
     || unlikely(*pvg != vg) /* display server bug */)
        vlc_wl_global_remove(vg);

    (void) registry;
}

static void registry_global_remove_cb(void *data, struct wl_registry *registry,
                                      uint32_t name)
{
    struct vlc_wl_registry *vr = data;
    struct vlc_wl_global key = { .name = name };
    void **pvg = tfind(&key, &vr->names, vwncmp);

    if (likely(pvg != NULL))
    {
        struct vlc_wl_global *vg = *pvg;

        tdelete(vg, &vr->names, vwncmp);
        vlc_wl_global_remove(vg);
    }

    (void) registry;
}

static const struct wl_registry_listener registry_cbs =
{
    registry_global_cb,
    registry_global_remove_cb,
};

struct vlc_wl_registry *vlc_wl_registry_get(struct wl_display *display,
                                            struct wl_event_queue *queue)
{
    struct vlc_wl_registry *vr = malloc(sizeof (*vr));
    if (unlikely(vr == NULL))
        return NULL;

    struct wl_display *wrapper = wl_proxy_create_wrapper(display);
    if (unlikely(wrapper == NULL))
        goto error;

    wl_proxy_set_queue((struct wl_proxy *)wrapper, queue);
    vr->registry = wl_display_get_registry(wrapper);
    wl_proxy_wrapper_destroy(wrapper);

    if (unlikely(vr->registry == NULL))
        goto error;

    vr->interfaces = NULL;
    vr->names = NULL;

    wl_registry_add_listener(vr->registry, &registry_cbs, vr);

     /* complete registry enumeration */
    if (wl_display_roundtrip_queue(display, queue) < 0)
    {
        vlc_wl_registry_destroy(vr);
        vr = NULL;
    }

    return vr;
error:
    free(vr);
    return vr;
}

static void name_destroy(void *d)
{
    vlc_wl_global_remove(d);
}

static void interface_destroy(void *d)
{
    vlc_wl_interface_destroy(d);
}

void vlc_wl_registry_destroy(struct vlc_wl_registry *vr)
{
    wl_registry_destroy(vr->registry);
    tdestroy(vr->names, name_destroy);
    tdestroy(vr->interfaces, interface_destroy);
    free(vr);
}

struct wl_compositor *vlc_wl_compositor_get(struct vlc_wl_registry *vr)
{
    return (struct wl_compositor *)vlc_wl_interface_bind(vr, "wl_compositor",
                                               &wl_compositor_interface, NULL);
}

struct wl_shm *vlc_wl_shm_get(struct vlc_wl_registry *vr)
{
    return (struct wl_shm *)vlc_wl_interface_bind(vr, "wl_shm",
                                                  &wl_shm_interface, NULL);
}
