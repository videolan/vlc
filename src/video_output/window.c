/*****************************************************************************
 * window.c: "vout window" management
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * $Id$
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
#include <math.h>
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

static int vout_window_start(void *func, va_list ap)
{
    int (*activate)(vout_window_t *, const vout_window_cfg_t *) = func;
    vout_window_t *wnd = va_arg(ap, vout_window_t *);
    const vout_window_cfg_t *cfg = va_arg(ap, const vout_window_cfg_t *);

    return activate(wnd, cfg);
}

vout_window_t *vout_window_New(vlc_object_t *obj, const char *module,
                               const vout_window_cfg_t *cfg,
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
                                vout_window_start, window, cfg);
    if (!w->module) {
        vlc_object_release(window);
        return NULL;
    }

    /* Hook for screensaver inhibition */
    if (var_InheritBool(obj, "disable-screensaver") &&
        (window->type == VOUT_WINDOW_TYPE_XID || window->type == VOUT_WINDOW_TYPE_HWND
      || window->type == VOUT_WINDOW_TYPE_WAYLAND))
    {
        w->inhibit = vlc_inhibit_Create(VLC_OBJECT (window));
        if (w->inhibit != NULL)
            vlc_inhibit_Set(w->inhibit, VLC_INHIBIT_VIDEO);
    }
    else
        w->inhibit = NULL;
    return window;
}

void vout_window_Delete(vout_window_t *window)
{
    if (!window)
        return;

    window_t *w = (window_t *)window;
    if (w->inhibit)
    {
        vlc_inhibit_Set (w->inhibit, VLC_INHIBIT_NONE);
        vlc_inhibit_Destroy (w->inhibit);
    }

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

static void vout_display_window_GetSize(vlc_object_t *obj,
                                        const video_format_t *restrict source,
                                        unsigned *restrict width,
                                        unsigned *restrict height)
{
    *width = var_InheritInteger(obj, "width");
    *height = var_InheritInteger(obj, "height");

    /* If both width and height are forced, keep them as is. */
    if (*width != (unsigned)-1 && *height != (unsigned)-1)
        return;

    /* Compute intended video resolution from source. */
    unsigned w = source->i_visible_width;
    unsigned h = source->i_visible_height;

    assert(source->i_sar_num > 0 && source->i_sar_den > 0);
    w = (w * source->i_sar_num) / source->i_sar_den;

    char *crop = var_InheritString(obj, "crop");
    if (crop != NULL)
    {
        unsigned num, den, cw, ch, top, bottom, left, right;

        if (sscanf(crop, "%u:%u", &num, &den) == 2 && num > 0 && den > 0) {
            if (w * den > h * num)
                w = h * num / den;
            else
                h = w * den / num;
        } else
        if (sscanf(crop, "%ux%u+%*u+%u", &cw, &ch, &(unsigned){ 0 }) == 3) {
            w = cw;
            h = ch;
        } else
        if (sscanf(crop, "%u+%u+%u+%u", &left, &top, &right, &bottom) == 4
         && right > left && bottom > top) {
            w = right - left;
            h = bottom - top;
        } else
            msg_Warn(obj, "Unknown crop format (%s)", crop);
        free(crop);
    }

    char *aspect = crop ? NULL : var_InheritString(obj, "aspect-ratio");
    if (aspect != NULL) {
        unsigned num, den;

        if (sscanf(aspect, "%u:%u", &num, &den) == 2 && num > 0 && den > 0)
            w = h * num / den;
        else
            msg_Warn(obj, "Unknown aspect format (%s)", aspect);
        free(aspect);
    }

    /* Adjust video size for orientation and pixel A/R. */
    if (ORIENT_IS_SWAP(source->orientation)) {
        unsigned x = w;

        w = h;
        h = x;
    }

    unsigned par_num, par_den;
    if (var_InheritURational(obj, &par_num, &par_den, "monitor-par") == 0
     && par_num > 0 && par_den > 0)
        w = (w * par_den) / par_num;

    /* If width is forced, adjust height according to the aspect ratio */
    if (*width != (unsigned)-1) {
        *height = (*width * h) / w;
        return;
    }

    /* If height is forced, adjust width according to the aspect ratio */
    if (*height != (unsigned)-1) {
        *width = (*height * w) / h;
        return;
    }

    /* If neither width nor height are forced, use the requested zoom. */
    float zoom = var_InheritFloat(obj, "zoom");

    if (isnan(zoom))
        zoom = 1.f;
    else
        zoom = fabsf(zoom);

    if (zoom < 0.1f)
        zoom = 0.1f;
    if (zoom > 10.f)
        zoom = 10.f;

    *width = lroundf(zoom * (float)w);
    *height = lroundf(zoom * (float)h);
}

typedef struct vout_display_window
{
    vlc_mouse_t mouse;
    vlc_tick_t last_left_press;
} vout_display_window_t;

static void vout_display_window_ResizeNotify(vout_window_t *window,
                                             unsigned width, unsigned height)
{
    vout_thread_t *vout = (vout_thread_t *)window->obj.parent;

    msg_Dbg(window, "resized to %ux%u", width, height);
    vout_ControlChangeDisplaySize(vout, width, height);
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
    var_SetInteger(window->obj.parent, "window-state", state);
}

static void vout_display_window_FullscreenNotify(vout_window_t *window,
                                                 const char *id)
{
    msg_Dbg(window, (id != NULL) ? "window set to fullscreen on %s"
                                 : "window set to fullscreen", id);
    var_SetString(window->obj.parent, "window-fullscreen-output",
                  (id != NULL) ? id : "");
    var_SetBool(window->obj.parent, "window-fullscreen", true);
}

static void vout_display_window_WindowingNotify(vout_window_t *window)
{
    msg_Dbg(window, "window set windowed");
    var_SetBool(window->obj.parent, "window-fullscreen", false);
}

static void vout_display_window_MouseEvent(vout_window_t *window,
                                           const vout_window_mouse_event_t *ev)
{
    vout_display_window_t *state = window->owner.sys;
    vout_thread_t *vout = (vout_thread_t *)window->obj.parent;
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
    var_SetInteger(window->obj.libvlc, "key-pressed", key);
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
vout_window_t *vout_display_window_New(vout_thread_t *vout,
                                       const vout_window_cfg_t *cfg)
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

    window = vout_window_New((vlc_object_t *)vout, modlist, cfg, &owner);
    free(modlist);
    if (window == NULL)
        free(state);
    return window;
}

void vout_display_window_UpdateSize(vout_window_t *window,
                                    const video_format_t *restrict fmt)
{
    unsigned width, height;

    vout_display_window_GetSize(VLC_OBJECT(window), fmt, &width, &height);
    if (width > 0 && height > 0) {
        msg_Dbg(window, "requested size: %ux%u", width, height);
        vout_window_SetSize(window, width, height);
    }
}

/**
 * Destroys a video window.
 * \note The window must be detached.
 */
void vout_display_window_Delete(vout_window_t *window)
{
    vout_thread_t *vout = (vout_thread_t *)(window->obj.parent);
    vout_display_window_t *state = window->owner.sys;

    vout_window_Delete(window);
    var_Destroy(vout, "window-fullscreen-output");
    var_Destroy(vout, "window-fullscreen");
    var_Destroy(vout, "window-state");
    free(state);
}
