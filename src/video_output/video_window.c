/*****************************************************************************
 * video_window.c: vout-specific window management
 *****************************************************************************
 * Copyright © 2014-2021 Rémi Denis-Courmont
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
# include "config.h"
#endif
#include <assert.h>
#include <stdlib.h>

#include <vlc_common.h>
#include <vlc_vout_window.h>
#include <vlc_vout.h>
#include <vlc_vout_display.h>
#include "video_window.h"
#include "vout_internal.h"

#define DOUBLE_CLICK_TIME VLC_TICK_FROM_MS(300)

struct vout_window_ack_data {
    vout_window_t *window;
    vout_window_ack_cb callback;
    unsigned width;
    unsigned height;
    void *opaque;
};

static void vout_window_Ack(void *data)
{
    struct vout_window_ack_data *cb_data = data;

    if (cb_data->callback != NULL)
        cb_data->callback(cb_data->window, cb_data->width, cb_data->height,
                          cb_data->opaque);
}

typedef struct vout_display_window
{
    vout_thread_t *vout;
    vlc_mouse_t mouse;
    vlc_tick_t last_left_press;
} vout_display_window_t;

static void vout_display_window_ResizeNotify(vout_window_t *window,
                                             unsigned width, unsigned height,
                                             vout_window_ack_cb cb,
                                             void *opaque)
{
    vout_display_window_t *state = window->owner.sys;
    vout_thread_t *vout = state->vout;
    struct vout_window_ack_data data = { window, cb, width, height, opaque };

    msg_Dbg(window, "resized to %ux%u", width, height);
    vout_ChangeDisplaySize(vout, width, height, vout_window_Ack, &data);
}

static void vout_display_window_CloseNotify(vout_window_t *window)
{
    /* TODO: Nowhere to dispatch to currently.
     * Needs callback to ES output to deselect ES? */
    msg_Err(window, "window closed");
}

static void vout_display_window_StateNotify(vout_window_t *window,
                                            unsigned window_state)
{
    vout_display_window_t *state = window->owner.sys;
    vout_thread_t *vout = state->vout;

    static const char states[][8] = {
        [VOUT_WINDOW_STATE_NORMAL] = "normal",
        [VOUT_WINDOW_STATE_ABOVE] = "above",
        [VOUT_WINDOW_STATE_BELOW] = "below",
    };

    assert(window_state < ARRAY_SIZE(states));
    msg_Dbg(window, "window state changed: %s", states[window_state]);
    var_SetInteger(vout, "window-state", window_state);
}

static void vout_display_window_FullscreenNotify(vout_window_t *window,
                                                 const char *id)
{
    vout_display_window_t *state = window->owner.sys;
    vout_thread_t *vout = state->vout;

    msg_Dbg(window, (id != NULL) ? "window set to fullscreen on %s"
                                 : "window set to fullscreen", id);
    var_SetString(vout, "window-fullscreen-output",
                  (id != NULL) ? id : "");
    var_SetBool(vout, "window-fullscreen", true);
}

static void vout_display_window_WindowingNotify(vout_window_t *window)
{
    vout_display_window_t *state = window->owner.sys;
    vout_thread_t *vout = state->vout;

    msg_Dbg(window, "window set windowed");
    var_SetBool(vout, "window-fullscreen", false);
}

static void vout_display_window_MouseEvent(vout_window_t *window,
                                           const vout_window_mouse_event_t *ev)
{
    vout_display_window_t *state = window->owner.sys;
    vout_thread_t *vout = state->vout;
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

void vout_display_SizeWindow(unsigned *restrict width,
                             unsigned *restrict height,
                             const video_format_t *restrict original,
                             const vlc_rational_t *restrict dar,
                             const struct vout_crop *restrict crop,
                             const vout_display_cfg_t *restrict cfg)
{
    unsigned w = original->i_visible_width;
    unsigned h = original->i_visible_height;
    unsigned sar_num = original->i_sar_num;
    unsigned sar_den = original->i_sar_den;

    if (dar->num > 0 && dar->den > 0) {
        unsigned num = dar->num * h;
        unsigned den = dar->den * w;

        vlc_ureduce(&sar_num, &sar_den, num, den, 0);
    }

    switch (crop->mode) {
        case VOUT_CROP_NONE:
            break;

        case VOUT_CROP_RATIO: {
            unsigned num = crop->ratio.num;
            unsigned den = crop->ratio.den;

            if (w * den > h * num)
                w = h * num / den;
            else
                h = w * den / num;
            break;
        }

        case VOUT_CROP_WINDOW:
            w = crop->window.width;
            h = crop->window.height;
            break;

        case VOUT_CROP_BORDER:
            w = crop->border.right - crop->border.left;
            h = crop->border.bottom - crop->border.top;
            break;
    }

    *width = cfg->display.width;
    *height = cfg->display.height;

    /* If both width and height are forced, keep them as is. */
    if (*width != 0 && *height != 0)
        return;

    /* Compute intended video resolution from source. */
    assert(sar_num > 0 && sar_den > 0);
    w = (w * sar_num) / sar_den;

    /* Adjust video size for orientation and pixel A/R. */
    if (ORIENT_IS_SWAP(original->orientation)) {
        unsigned x = w;

        w = h;
        h = x;
    }

    if (cfg->display.sar.num > 0 && cfg->display.sar.den > 0)
        w = (w * cfg->display.sar.den) / cfg->display.sar.num;

    /* If width is forced, adjust height according to the aspect ratio */
    if (*width != 0) {
        *height = (*width * h) / w;
        return;
    }

    /* If height is forced, adjust width according to the aspect ratio */
    if (*height != 0) {
        *width = (*height * w) / h;
        return;
    }

    /* If neither width nor height are forced, use the requested zoom. */
    *width = (w * cfg->zoom.num) / cfg->zoom.den;
    *height = (h * cfg->zoom.num) / cfg->zoom.den;
}

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
    state->vout = vout;

    char *modlist = var_InheritString(vout, "window");
    vout_window_owner_t owner = {
        .cbs = &vout_display_window_cbs,
        .sys = state,
    };
    vout_window_cfg_t cfg = {
        .is_fullscreen = var_GetBool(&vout->obj, "fullscreen"),
        .is_decorated = var_InheritBool(&vout->obj, "video-deco"),
#if defined(__APPLE__) || defined(_WIN32)
        .x = var_InheritInteger(&vout->obj, "video-x"),
        .y = var_InheritInteger(&vout->obj, "video-y"),
#endif
    };
    vout_window_t *window;

    var_Create(vout, "window-state", VLC_VAR_INTEGER);
    var_Create(vout, "window-fullscreen", VLC_VAR_BOOL);
    var_Create(vout, "window-fullscreen-output", VLC_VAR_STRING);

    window = vout_window_New((vlc_object_t *)vout, modlist, &owner, &cfg);
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
    vout_display_window_t *state = window->owner.sys;
    vout_thread_t *vout = state->vout;

    vout_window_Delete(window);
    var_Destroy(vout, "window-fullscreen-output");
    var_Destroy(vout, "window-fullscreen");
    var_Destroy(vout, "window-state");
    free(state);
}
