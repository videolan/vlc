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
#include <sys/mman.h>
#include <linux/input-event-codes.h>
#include <wayland-client.h>
#ifdef HAVE_XKBCOMMON
# include <xkbcommon/xkbcommon.h>
# include "../xcb/vlc_xkb.h"
#endif
#include <vlc_common.h>
#include <vlc_vout_window.h>
#include <vlc_mouse.h>
#include <vlc_fs.h>

#include "input.h"

struct seat_data
{
    vout_window_t *owner;
    struct wl_seat *seat;

    struct wl_pointer *pointer;
    vlc_tick_t cursor_timeout;
    vlc_tick_t cursor_deadline;
    uint32_t cursor_serial;

#ifdef HAVE_XKBCOMMON
    struct xkb_context *xkb;
    struct wl_keyboard *keyboard;
    struct xkb_keymap *keymap;
    struct xkb_state *keystate;
#endif

    uint32_t name;
    uint32_t version;
    struct wl_list node;
};

static void pointer_show(struct seat_data *sd, struct wl_pointer *pointer)
{
    int hsx, hsy;
    struct wl_surface *surface = window_get_cursor(sd->owner, &hsx, &hsy);

    if (surface != NULL)
    {
        wl_pointer_set_cursor(pointer, sd->cursor_serial, surface, hsx, hsy);
        sd->cursor_deadline = vlc_tick_now() + sd->cursor_timeout;
    }
}

static void pointer_enter_cb(void *data, struct wl_pointer *pointer,
                             uint32_t serial, struct wl_surface *surface,
                             wl_fixed_t sx, wl_fixed_t sy)
{
    struct seat_data *sd = data;

    sd->cursor_serial = serial;
    pointer_show(sd, pointer);
    (void) surface; (void) sx; (void) sy;
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

    pointer_show(sd, pointer);
    vout_window_ReportMouseMoved(sd->owner,
                                 wl_fixed_to_int(sx), wl_fixed_to_int(sy));
    (void) time;
}

static void pointer_button_cb(void *data, struct wl_pointer *pointer,
                              uint32_t serial, uint32_t time,
                              uint32_t keycode, uint32_t state)
{
    struct seat_data *sd = data;
    int button;

    pointer_show(sd, pointer);

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

    (void) serial; (void) time;
}

static void pointer_axis_cb(void *data, struct wl_pointer *pointer,
                            uint32_t serial, uint32_t type, wl_fixed_t value)
{
    struct seat_data *sd = data;
    vout_window_t *wnd = sd->owner;
    int button;
    bool plus = value > 0;

    pointer_show(sd, pointer);
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
    (void) serial;
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

    sd->cursor_timeout = VLC_TICK_FROM_MS( var_InheritInteger(sd->owner, "mouse-hide-timeout") );
    sd->cursor_deadline = INT64_MAX;
}

static void pointer_destroy(struct seat_data *sd)
{
    if (sd->pointer == NULL)
        return;

    if (sd->version >= WL_POINTER_RELEASE_SINCE_VERSION)
        wl_pointer_release(sd->pointer);
    else
        wl_pointer_destroy(sd->pointer);

    sd->pointer = NULL;
}

#ifdef HAVE_XKBCOMMON
static void keyboard_keymap_cb(void *data, struct wl_keyboard *keyboard,
                               uint32_t format, int fd, uint32_t size)
{
    struct seat_data *sd = data;
    vout_window_t *wnd = sd->owner;
    void *map;

    msg_Dbg(wnd, "format %"PRIu32" keymap of %"PRIu32" bytes", format, size);

    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1)
    {
        msg_Err(wnd, "unsupported keymap format %"PRIu32, format);
        goto out;
    }

    size++; /* trailing nul */

    map = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED)
        goto out;

    assert(((char *)map)[size - 1] == '\0');
    sd->keymap = xkb_keymap_new_from_string(sd->xkb, map,
                                            XKB_KEYMAP_FORMAT_TEXT_V1,
                                            XKB_KEYMAP_COMPILE_NO_FLAGS);
    munmap(map, size);

    if (sd->keymap != NULL)
        sd->keystate = xkb_state_new(sd->keymap);
    else
        msg_Err(wnd, "keymap parse error");

out:
    vlc_close(fd);
    (void) keyboard;
}

static void keyboard_enter_cb(void *data, struct wl_keyboard *keyboard,
                              uint32_t serial, struct wl_surface *surface,
                              struct wl_array *keys)
{
    (void) data; (void) keyboard; (void) serial; (void) surface; (void) keys;
}

static void keyboard_leave_cb(void *data, struct wl_keyboard *keyboard,
                              uint32_t serial, struct wl_surface *surface)
{
    (void) data; (void) keyboard; (void) serial; (void) surface;}

static void keyboard_key_cb(void *data, struct wl_keyboard *keyboard,
                            uint32_t serial, uint32_t time, uint32_t keycode,
                            uint32_t state)
{
    struct seat_data *sd = data;
    vout_window_t *wnd = sd->owner;
    uint_fast32_t vk;

    if (state != WL_KEYBOARD_KEY_STATE_PRESSED)
        return;
    if (unlikely(sd->keystate == NULL))
        return;

    vk = vlc_xkb_get_one(sd->keystate, keycode + 8);
    if (vk)
    {
        msg_Dbg(wnd, "key: 0x%08"PRIxFAST32" (XKB: 0x%04"PRIx32")",
                vk, keycode);
        vout_window_ReportKeyPress(wnd, vk);
    }

    (void) keyboard; (void) serial; (void) time;
}

static void keyboard_modifiers_cb(void *data, struct wl_keyboard *keyboard,
                                  uint32_t serial, uint32_t depressed,
                                  uint32_t latched, uint32_t locked,
                                  uint32_t group)
{
    struct seat_data *sd = data;

    if (unlikely(sd->keystate == NULL))
        return;

    xkb_state_update_mask(sd->keystate, depressed, latched, locked,
                          0, 0, group);

    (void) keyboard; (void) serial;
}

static void keyboard_repeat_info_cb(void *data, struct wl_keyboard *keyboard,
                                    int32_t rate, int32_t delay)
{
    struct seat_data *sd = data;
    vout_window_t *wnd = sd->owner;

    msg_Dbg(wnd, "keyboard repeat info: %d Hz after %d ms", rate, delay);
    (void) keyboard;
}

static const struct wl_keyboard_listener keyboard_cbs =
{
    keyboard_keymap_cb,
    keyboard_enter_cb,
    keyboard_leave_cb,
    keyboard_key_cb,
    keyboard_modifiers_cb,
    keyboard_repeat_info_cb,
};

static void keyboard_create(struct seat_data *sd)
{
    if (sd->keyboard != NULL)
        return;

    sd->keyboard = wl_seat_get_keyboard(sd->seat);
    if (likely(sd->keyboard != NULL))
    {
        sd->keymap = NULL;
        wl_keyboard_add_listener(sd->keyboard, &keyboard_cbs, sd);
    }
}

static void keyboard_destroy(struct seat_data *sd)
{
    if (sd->keyboard == NULL)
        return;

    if (sd->version >= WL_KEYBOARD_RELEASE_SINCE_VERSION)
        wl_keyboard_release(sd->keyboard);
    else
        wl_keyboard_destroy(sd->keyboard);

    if (sd->keymap != NULL)
    {
        if (sd->keystate != NULL)
            xkb_state_unref(sd->keystate);
        xkb_keymap_unref(sd->keymap);
    }

    sd->keyboard = NULL;
}
#endif /* HAVE_XKBCOMMON */

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

#ifdef HAVE_XKBCOMMON
    if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD)
    {
        if (sd->xkb != NULL)
            keyboard_create(sd);
    }
    else
        keyboard_destroy(sd);
#endif
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

    sd->seat = wl_registry_bind(registry, name, &wl_seat_interface, version);
    if (unlikely(sd->seat == NULL))
    {
        free(sd);
        return -1;
    }

    sd->owner = wnd;
    sd->pointer = NULL;
#ifdef HAVE_XKBCOMMON
    if (var_InheritBool(wnd, "keyboard-events"))
        sd->xkb = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

    sd->keyboard = NULL;
#endif
    sd->version = version;

    wl_seat_add_listener(sd->seat, &seat_cbs, sd);
    wl_list_insert(list, &sd->node);
    return 0;
}

static vlc_tick_t seat_next_deadline(const struct seat_data *sd)
{
    return (sd->pointer != NULL) ? sd->cursor_deadline : INT64_MAX;
}

static void seat_refresh(struct seat_data *sd, vlc_tick_t now)
{
    if (sd->pointer != NULL && sd->cursor_deadline <= now)
    {   /* Hide cursor */
        wl_pointer_set_cursor(sd->pointer, sd->cursor_serial, NULL, 0, 0);
        sd->cursor_deadline = INT64_MAX;
    }
}

static void seat_destroy(struct seat_data *sd)
{
    wl_list_remove(&sd->node);

#ifdef HAVE_XKBCOMMON
    keyboard_destroy(sd);

    if (sd->xkb != NULL)
        xkb_context_unref(sd->xkb);
#endif
    pointer_destroy(sd);

    if (sd->version >= WL_SEAT_RELEASE_SINCE_VERSION)
        wl_seat_release(sd->seat);
    else
        wl_seat_destroy(sd->seat);
    free(sd);
}

int seat_destroy_one(struct wl_list *list, uint32_t name)
{
    struct seat_data *sd;

    wl_list_for_each(sd, list, node)
    {
        if (sd->name == name)
        {
            seat_destroy(sd);
            /* Note: return here so no needs for safe walk variant */
            return 0;
        }
    }

    return -1;
}

void seat_destroy_all(struct wl_list *list)
{
    while (!wl_list_empty(list))
        seat_destroy(container_of(list->next, struct seat_data, node));
}

int seat_next_timeout(const struct wl_list *list)
{
    struct seat_data *sd;
    vlc_tick_t deadline = INT64_MAX;

    wl_list_for_each(sd, list, node)
    {
        vlc_tick_t d = seat_next_deadline(sd);
        if (deadline > d)
            deadline = d;
    }

    if (deadline == INT64_MAX)
        return -1;

    vlc_tick_t now = vlc_tick_now();
    if (now >= deadline)
        return 0;

    return (deadline - now) / 1000 + 1;
}

void seat_timeout(struct wl_list *list)
{
    struct seat_data *sd;
    vlc_tick_t now = vlc_tick_now();

    wl_list_for_each(sd, list, node)
        seat_refresh(sd, now);
}
