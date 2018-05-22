/**
 * @file input.c
 * @brief Wayland input events for VLC media player
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
#include <inttypes.h>
#include <stdlib.h>
#include <wayland-client.h>
#include <vlc_common.h>
#include <vlc_vout_window.h>

#include "input.h"

struct seat_data
{
    vout_window_t *owner;
    struct wl_seat *seat;

    uint32_t version;
    struct wl_list node;
};

static void seat_capabilities_cb(void *data, struct wl_seat *seat,
                                 uint32_t capabilities)
{
    struct seat_data *sd = data;
    struct vout_window_t *wnd = sd->owner;

    msg_Dbg(wnd, "seat capabilities: 0x%"PRIx32, capabilities);
    (void) seat;
}

static void seat_name_cb(void *data, struct wl_seat *seat, const char *name)
{
    struct seat_data *sd = data;

    msg_Dbg(sd->owner, "seat name: %s", name);
    (void) seat;
}

static const struct wl_seat_listener seat_cbs =
{
    seat_capabilities_cb,
    seat_name_cb,
};

int seat_create(vout_window_t *wnd, struct wl_registry *registry,
                uint32_t name, uint32_t version, struct wl_list *list)
{
    struct seat_data *sd = malloc(sizeof (*sd));
    if (unlikely(sd == NULL))
        return -1;

    if (version > 5)
        version = 5;

    sd->owner = wnd;
    sd->version = version;

    sd->seat = wl_registry_bind(registry, name, &wl_seat_interface, version);
    if (unlikely(sd->seat == NULL))
    {
        free(sd);
        return -1;
    }

    wl_seat_add_listener(sd->seat, &seat_cbs, sd);
    wl_list_insert(list, &sd->node);
    return 0;
}

static void seat_destroy(struct seat_data *sd)
{
    wl_list_remove(&sd->node);

    if (sd->version >= WL_SEAT_RELEASE_SINCE_VERSION)
        wl_seat_release(sd->seat);
    else
        wl_seat_destroy(sd->seat);
    free(sd);
}

void seat_destroy_all(struct wl_list *list)
{
    while (!wl_list_empty(list))
        seat_destroy(container_of(list->next, struct seat_data, node));
}
