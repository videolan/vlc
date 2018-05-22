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
#include <linux/input-event-codes.h>
#include <wayland-client.h>
#include <vlc_common.h>
#include <vlc_vout_window.h>
#include <vlc_mouse.h>

#include "input.h"

struct seat_data
{
    vout_window_t *owner;
    struct wl_seat *seat;
    struct wl_pointer *pointer;

    uint32_t version;
    struct wl_list node;
};

static void pointer_enter_cb(void *data, struct wl_pointer *pointer,
                             uint32_t serial, struct wl_surface *surface,
                             wl_fixed_t sx, wl_fixed_t sy)
{
    (void) data; (void) pointer; (void) serial; (void) surface;
    (void) sx; (void) sy; /* TODO: set_cursor */
}

static void pointer_leave_cb(void *data, struct wl_pointer *pointer,
                             uint32_t serial, struct wl_surface *surface)
{
    (void) data; (void) pointer; (void) serial; (void) surface;
}

static void pointer_motion_cb(void *data, struct wl_pointer *pointer,
                              uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
    struct seat_data *sd = data;

    vout_window_ReportMouseMoved(sd->owner,
                                 wl_fixed_to_int(sx), wl_fixed_to_int(sy));
    (void) pointer; (void) time;
}

static void pointer_button_cb(void *data, struct wl_pointer *pointer,
                              uint32_t serial, uint32_t time,
                              uint32_t keycode, uint32_t state)
{
    struct seat_data *sd = data;
    int button;

    switch (keycode)
    {
        case BTN_LEFT:
            button = MOUSE_BUTTON_LEFT;
            break;
        case BTN_RIGHT:
            button = MOUSE_BUTTON_RIGHT;
            break;
        case BTN_MIDDLE:
            button = MOUSE_BUTTON_CENTER;
            break;
        default:
            return;
    }

    switch (state)
    {
        case WL_POINTER_BUTTON_STATE_RELEASED:
            vout_window_ReportMouseReleased(sd->owner, button);
            break;
        case WL_POINTER_BUTTON_STATE_PRESSED:
            vout_window_ReportMousePressed(sd->owner, button);
            break;
    }

    (void) pointer; (void) serial; (void) time;
}

static void pointer_axis_cb(void *data, struct wl_pointer *pointer,
                            uint32_t serial, uint32_t type, wl_fixed_t value)
{
    struct seat_data *sd = data;
    vout_window_t *wnd = sd->owner;
    int button;
    bool plus = value > 0;

    value = abs(value);

    switch (type)
    {
        case WL_POINTER_AXIS_VERTICAL_SCROLL:
            button = plus ? MOUSE_BUTTON_WHEEL_DOWN : MOUSE_BUTTON_WHEEL_UP;
            break;
        case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
            button = plus ? MOUSE_BUTTON_WHEEL_RIGHT : MOUSE_BUTTON_WHEEL_LEFT;
            break;
        default:
            return;
    }

    while (value > 0)
    {
        vout_window_ReportMousePressed(wnd, button);
        vout_window_ReportMouseReleased(wnd, button);
        value -= wl_fixed_from_int(10);
    }
    (void) pointer; (void) serial;
}

static void pointer_frame_cb(void *data, struct wl_pointer *pointer)
{
    (void) data; (void) pointer;
}

static void pointer_axis_source(void *data, struct wl_pointer *pointer,
                                uint32_t source)
{
    (void) data; (void) pointer; (void) source;
}

static void pointer_axis_stop(void *data, struct wl_pointer *pointer,
                              uint32_t time, uint32_t axis)
{
    (void) data; (void) pointer; (void) time; (void) axis;
}

static void pointer_axis_discrete(void *data, struct wl_pointer *pointer,
                                  uint32_t type, int steps)
{
    (void) data; (void) pointer; (void) type; (void) steps;
}

static const struct wl_pointer_listener pointer_cbs =
{
    pointer_enter_cb,
    pointer_leave_cb,
    pointer_motion_cb,
    pointer_button_cb,
    pointer_axis_cb,
    pointer_frame_cb,
    pointer_axis_source,
    pointer_axis_stop,
    pointer_axis_discrete,
};

static void pointer_create(struct seat_data *sd)
{
    if (sd->pointer != NULL)
        return;

    sd->pointer = wl_seat_get_pointer(sd->seat);
    if (likely(sd->pointer != NULL))
        wl_pointer_add_listener(sd->pointer, &pointer_cbs, sd);
}

static void pointer_destroy(struct seat_data *sd)
{
    if (sd->pointer == NULL)
        return;

    if (sd->version >= WL_POINTER_RELEASE_SINCE_VERSION)
        wl_pointer_release(sd->pointer);
    else
        wl_pointer_destroy(sd->pointer);
}

static void seat_capabilities_cb(void *data, struct wl_seat *seat,
                                 uint32_t capabilities)
{
    struct seat_data *sd = data;
    struct vout_window_t *wnd = sd->owner;

    msg_Dbg(wnd, "seat capabilities: 0x%"PRIx32, capabilities);
    (void) seat;

    if (capabilities & WL_SEAT_CAPABILITY_POINTER)
    {
        if (var_InheritBool(wnd, "mouse-events"))
            pointer_create(sd);
    }
    else
        pointer_destroy(sd);
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
    sd->pointer = NULL;
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

    pointer_destroy(sd);

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
