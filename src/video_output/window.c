/*****************************************************************************
 * window.c: "vout window" management
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <vlc_common.h>
#include <vlc_vout_window.h>
#include <vlc_modules.h>
#include "inhibit.h"
#include <libvlc.h>

typedef struct
{
    vout_window_t wnd;
    module_t *module;
    vlc_inhibit_t *inhibit;
} window_t;

static int vout_window_start(void *func, bool forced, va_list ap)
{
    int (*activate)(vout_window_t *) = func;
    vout_window_t *wnd = va_arg(ap, vout_window_t *);

    (void) forced;
    return activate(wnd);
}

vout_window_t *vout_window_New(vlc_object_t *obj, const char *module,
                               const vout_window_owner_t *owner)
{
    window_t *w = vlc_custom_create(obj, sizeof(*w), "window");
    vout_window_t *window = &w->wnd;

    memset(&window->handle, 0, sizeof(window->handle));
    window->info.has_double_click = false;
    window->sys = NULL;
    assert(owner != NULL);
    window->owner = *owner;

    w->module = vlc_module_load(window, "vout window", module, false,
                                vout_window_start, window);
    if (!w->module) {
        vlc_object_release(window);
        return NULL;
    }

    /* Hook for screensaver inhibition */
    if (var_InheritBool(obj, "disable-screensaver") &&
        (window->type == VOUT_WINDOW_TYPE_XID || window->type == VOUT_WINDOW_TYPE_HWND
      || window->type == VOUT_WINDOW_TYPE_WAYLAND))
        w->inhibit = vlc_inhibit_Create(VLC_OBJECT(window));
    else
        w->inhibit = NULL;
    return window;
}

int vout_window_Enable(vout_window_t *window,
                       const vout_window_cfg_t *restrict cfg)
{
    window_t *w = container_of(window, window_t, wnd);

    if (window->ops->enable != NULL) {
        int err = window->ops->enable(window, cfg);
        if (err)
            return err;
    }

    if (w->inhibit != NULL)
        vlc_inhibit_Set(w->inhibit, VLC_INHIBIT_VIDEO);

    return VLC_SUCCESS;
}

void vout_window_Disable(vout_window_t *window)
{
    window_t *w = container_of(window, window_t, wnd);

    if (w->inhibit != NULL)
        vlc_inhibit_Set(w->inhibit, VLC_INHIBIT_NONE);

    if (window->ops->disable != NULL)
        window->ops->disable(window);
}

void vout_window_Delete(vout_window_t *window)
{
    if (!window)
        return;

    window_t *w = container_of(window, window_t, wnd);

    if (w->inhibit != NULL)
        vlc_inhibit_Destroy(w->inhibit);
    if (window->ops->destroy != NULL)
        window->ops->destroy(window);
    vlc_objres_clear(VLC_OBJECT(window));
    vlc_object_release(window);
}

void vout_window_SetInhibition(vout_window_t *window, bool enabled)
{
    window_t *w = (window_t *)window;
    unsigned flags = enabled ? VLC_INHIBIT_VIDEO : VLC_INHIBIT_NONE;

    if (w->inhibit != NULL)
        vlc_inhibit_Set(w->inhibit, flags);
}

/* Video output display integration */
#include <vlc_vout.h>
#include <vlc_vout_display.h>
#include "window.h"
#include "vout_internal.h"

#define DOUBLE_CLICK_TIME VLC_TICK_FROM_MS(300)

typedef struct vout_display_window
{
    vlc_mouse_t mouse;
    vlc_tick_t last_left_press;
} vout_display_window_t;

static void vout_display_window_ResizeNotify(vout_window_t *window,
                                             unsigned width, unsigned height)
{
    vout_thread_t *vout = (vout_thread_t *)vlc_object_parent(window);

    msg_Dbg(window, "resized to %ux%u", width, height);
    vout_ChangeDisplaySize(vout, width, height);
}

static void vout_display_window_CloseNotify(vout_window_t *window)
{
    /* TODO: Nowhere to dispatch to currently.
     * Needs callback to ES output to deselect ES? */
    msg_Err(window, "window closed");
}

static void vout_display_window_StateNotify(vout_window_t *window,
                                            unsigned state)
{
    static const char states[][8] = {
        [VOUT_WINDOW_STATE_NORMAL] = "normal",
        [VOUT_WINDOW_STATE_ABOVE] = "above",
        [VOUT_WINDOW_STATE_BELOW] = "below",
    };

    assert(state < ARRAY_SIZE(states));
    msg_Dbg(window, "window state changed: %s", states[state]);
    var_SetInteger(vlc_object_parent(window), "window-state", state);
}

static void vout_display_window_FullscreenNotify(vout_window_t *window,
                                                 const char *id)
{
    msg_Dbg(window, (id != NULL) ? "window set to fullscreen on %s"
                                 : "window set to fullscreen", id);
    var_SetString(vlc_object_parent(window), "window-fullscreen-output",
                  (id != NULL) ? id : "");
    var_SetBool(vlc_object_parent(window), "window-fullscreen", true);
}

static void vout_display_window_WindowingNotify(vout_window_t *window)
{
    msg_Dbg(window, "window set windowed");
    var_SetBool(vlc_object_parent(window), "window-fullscreen", false);
}

static void vout_display_window_MouseEvent(vout_window_t *window,
                                           const vout_window_mouse_event_t *ev)
{
    vout_display_window_t *state = window->owner.sys;
    vout_thread_t *vout = (vout_thread_t *)vlc_object_parent(window);
    vlc_mouse_t *m = &state->mouse;

    m->b_double_click = false;

    switch (ev->type)
    {
        case VOUT_WINDOW_MOUSE_MOVED:
            vlc_mouse_SetPosition(m, ev->x, ev->y);
            state->last_left_press = INT64_MIN;
            break;

        case VOUT_WINDOW_MOUSE_PRESSED:
            if (!window->info.has_double_click
             && ev->button_mask == MOUSE_BUTTON_LEFT
             && !vlc_mouse_IsLeftPressed(m))
            {
                const vlc_tick_t now = vlc_tick_now();

                if (state->last_left_press != INT64_MIN
                 && now - state->last_left_press < DOUBLE_CLICK_TIME)
                {
                    m->b_double_click = true;
                    state->last_left_press = INT64_MIN;
                }
                else
                    state->last_left_press = now;
            }

            vlc_mouse_SetPressed(m, ev->button_mask);
            break;

        case VOUT_WINDOW_MOUSE_RELEASED:
            vlc_mouse_SetReleased(m, ev->button_mask);
            break;

        case VOUT_WINDOW_MOUSE_DOUBLE_CLICK:
            assert(window->info.has_double_click);
            m->b_double_click = true;
            break;

        default:
            vlc_assert_unreachable();
    }

    vout_MouseState(vout, m);
}

static void vout_display_window_KeyboardEvent(vout_window_t *window,
                                              unsigned key)
{
    var_SetInteger(vlc_object_instance(window), "key-pressed", key);
}

static void vout_display_window_OutputEvent(vout_window_t *window,
                                            const char *name, const char *desc)
{
    if (desc != NULL)
        msg_Dbg(window, "fullscreen output %s (%s) added", name, desc);
    else
        msg_Dbg(window, "fullscreen output %s removed", name);
}

static const struct vout_window_callbacks vout_display_window_cbs = {
    .resized = vout_display_window_ResizeNotify,
    .closed = vout_display_window_CloseNotify,
    .state_changed = vout_display_window_StateNotify,
    .fullscreened = vout_display_window_FullscreenNotify,
    .windowed = vout_display_window_WindowingNotify,
    .mouse_event = vout_display_window_MouseEvent,
    .keyboard_event = vout_display_window_KeyboardEvent,
    .output_event = vout_display_window_OutputEvent,
};

/**
 * Creates a video window, initially without any attached display.
 */
vout_window_t *vout_display_window_New(vout_thread_t *vout)
{
    vout_display_window_t *state = malloc(sizeof (*state));
    if (state == NULL)
        return NULL;

    vlc_mouse_Init(&state->mouse);
    state->last_left_press = INT64_MIN;

    char *modlist = var_InheritString(vout, "window");
    vout_window_owner_t owner = {
        .cbs = &vout_display_window_cbs,
        .sys = state,
    };
    vout_window_t *window;

    var_Create(vout, "window-state", VLC_VAR_INTEGER);
    var_Create(vout, "window-fullscreen", VLC_VAR_BOOL);
    var_Create(vout, "window-fullscreen-output", VLC_VAR_STRING);

    window = vout_window_New((vlc_object_t *)vout, modlist, &owner);
    free(modlist);
    if (window == NULL)
        free(state);
    return window;
}

/**
 * Destroys a video window.
 * \note The window must be detached.
 */
void vout_display_window_Delete(vout_window_t *window)
{
    vout_thread_t *vout = (vout_thread_t *)vlc_object_parent(window);
    vout_display_window_t *state = window->owner.sys;

    vout_window_Disable(window);
    vout_window_Delete(window);
    var_Destroy(vout, "window-fullscreen-output");
    var_Destroy(vout, "window-fullscreen");
    var_Destroy(vout, "window-state");
    free(state);
}
