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
#include <vlc_window.h>
#include <vlc_vout.h>
#include <vlc_vout_display.h>
#include "video_window.h"
#include "vout_internal.h"

#define DOUBLE_CLICK_TIME VLC_TICK_FROM_MS(300)

struct vlc_window_ack_data {
    vlc_window_t *window;
    vlc_window_ack_cb callback;
    unsigned width;
    unsigned height;
    void *opaque;
};

static void vlc_window_Ack(void *data)
{
    struct vlc_window_ack_data *cb_data = data;

    if (cb_data->callback != NULL)
        cb_data->callback(cb_data->window, cb_data->width, cb_data->height,
                          cb_data->opaque);
}

typedef struct vout_display_window
{
    vout_thread_t *vout;

    video_format_t format;
    struct vout_display_placement display;
    vlc_mutex_t lock;

    struct {
        vlc_mouse_t window;
        vlc_mouse_t video;
        vlc_tick_t last_left_press;
        vlc_mouse_event event;
        void *opaque;
    } mouse;
} vout_display_window_t;

static void vout_display_window_ResizeNotify(vlc_window_t *window,
                                             unsigned width, unsigned height,
                                             vlc_window_ack_cb cb,
                                             void *opaque)
{
    vout_display_window_t *state = window->owner.sys;
    vout_thread_t *vout = state->vout;
    struct vlc_window_ack_data data = { window, cb, width, height, opaque };

    vlc_mutex_lock(&state->lock);
    state->display.width = width;
    state->display.height = height;
    vlc_mutex_unlock(&state->lock);

    msg_Dbg(window, "resized to %ux%u", width, height);
    vout_ChangeDisplaySize(vout, width, height, vlc_window_Ack, &data);
}

static void vout_display_window_CloseNotify(vlc_window_t *window)
{
    /* TODO: Nowhere to dispatch to currently.
     * Needs callback to ES output to deselect ES? */
    msg_Err(window, "window closed");
}

static void vout_display_window_StateNotify(vlc_window_t *window,
                                            unsigned window_state)
{
    vout_display_window_t *state = window->owner.sys;
    vout_thread_t *vout = state->vout;

    static const char states[][8] = {
        [VLC_WINDOW_STATE_NORMAL] = "normal",
        [VLC_WINDOW_STATE_ABOVE] = "above",
        [VLC_WINDOW_STATE_BELOW] = "below",
    };

    assert(window_state < ARRAY_SIZE(states));
    msg_Dbg(window, "window state changed: %s", states[window_state]);
    var_SetInteger(vout, "window-state", window_state);
}

static void vout_display_window_FullscreenNotify(vlc_window_t *window,
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

static void vout_display_window_WindowingNotify(vlc_window_t *window)
{
    vout_display_window_t *state = window->owner.sys;
    vout_thread_t *vout = state->vout;

    msg_Dbg(window, "window set windowed");
    var_SetBool(vout, "window-fullscreen", false);
}

static void vout_display_window_MouseEvent(vlc_window_t *window,
                                           const vlc_window_mouse_event_t *ev)
{
    vout_display_window_t *state = window->owner.sys;
    vout_thread_t *vout = state->vout;
    vlc_mouse_t *m = &state->mouse.window;

    m->b_double_click = false;

    switch (ev->type)
    {
        case VLC_WINDOW_MOUSE_MOVED:
            vlc_mouse_SetPosition(m, ev->x, ev->y);
            state->mouse.last_left_press = INT64_MIN;
            break;

        case VLC_WINDOW_MOUSE_PRESSED:
            if (!window->info.has_double_click
             && ev->button_mask == MOUSE_BUTTON_LEFT
             && !vlc_mouse_IsLeftPressed(m))
            {
                const vlc_tick_t now = vlc_tick_now();

                if (state->mouse.last_left_press != INT64_MIN
                 && now - state->mouse.last_left_press < DOUBLE_CLICK_TIME)
                {
                    m->b_double_click = true;
                    state->mouse.last_left_press = INT64_MIN;
                }
                else
                    state->mouse.last_left_press = now;
            }

            vlc_mouse_SetPressed(m, ev->button_mask);
            break;

        case VLC_WINDOW_MOUSE_RELEASED:
            vlc_mouse_SetReleased(m, ev->button_mask);
            break;

        case VLC_WINDOW_MOUSE_DOUBLE_CLICK:
            assert(window->info.has_double_click);
            m->b_double_click = true;
            break;

        default:
            vlc_assert_unreachable();
    }

    vlc_mouse_t video_mouse = *m;

    vlc_mutex_lock(&state->lock);
    if (likely(state->format.i_visible_width != 0
            && state->format.i_visible_height != 0
            && state->display.width != 0 && state->display.height != 0))
        vout_display_TranslateCoordinates(&video_mouse.i_x, &video_mouse.i_y,
                                          &state->format, &state->display);
    vlc_mutex_unlock(&state->lock);

    vout_FilterMouse(vout, &video_mouse);

    /* Check if the mouse state actually changed and emit events. */
    /* NOTE: sys->mouse is only used here, so no need to lock. */
    if (vlc_mouse_HasMoved(&state->mouse.video, &video_mouse))
        var_SetCoords(vout, "mouse-moved", m->i_x, m->i_y);
    if (vlc_mouse_HasButton(&state->mouse.video, &video_mouse))
        var_SetInteger(vout, "mouse-button-down", m->i_pressed);
    if (m->b_double_click)
        var_ToggleBool(vout, "fullscreen");

    state->mouse.video = video_mouse;

    vlc_mutex_lock(&state->lock);
    /* Mouse events are only initialised if the display exists. */
    if (state->mouse.event != NULL)
        state->mouse.event(&video_mouse, state->mouse.opaque);
    vlc_mutex_unlock(&state->lock);
}

static void vout_display_window_KeyboardEvent(vlc_window_t *window,
                                              unsigned key)
{
    var_SetInteger(vlc_object_instance(window), "key-pressed", key);
}

static void vout_display_window_OutputEvent(vlc_window_t *window,
                                            const char *name, const char *desc)
{
    if (desc != NULL)
        msg_Dbg(window, "fullscreen output %s (%s) added", name, desc);
    else
        msg_Dbg(window, "fullscreen output %s removed", name);
}

static void vout_display_window_IccEvent(vlc_window_t *window,
                                         vlc_icc_profile_t *profile)
{
    vout_display_window_t *state = window->owner.sys;
    vout_thread_t *vout = state->vout;

    if (profile != NULL)
        msg_Dbg(window, "window got ICC profile (%zu bytes)", profile->size);
    else
        msg_Dbg(window, "window ICC profile cleared");

    vout_ChangeIccProfile(vout, profile);
}

static const struct vlc_window_callbacks vout_display_window_cbs = {
    .resized = vout_display_window_ResizeNotify,
    .closed = vout_display_window_CloseNotify,
    .state_changed = vout_display_window_StateNotify,
    .fullscreened = vout_display_window_FullscreenNotify,
    .windowed = vout_display_window_WindowingNotify,
    .mouse_event = vout_display_window_MouseEvent,
    .keyboard_event = vout_display_window_KeyboardEvent,
    .output_event = vout_display_window_OutputEvent,
    .icc_event = vout_display_window_IccEvent,
};

void vout_display_window_SetMouseHandler(vlc_window_t *window,
                                         vlc_mouse_event event, void *opaque)
{
    vout_display_window_t *state = window->owner.sys;

    /*
     * Note that the current implementation of this function is technically
     * reentrant, but better not rely on this in calling code.
     */
    vlc_mutex_lock(&state->lock);
    if (state->mouse.event != NULL)
        state->mouse.event(NULL, state->mouse.opaque);

    state->mouse.event = event;
    state->mouse.opaque = opaque;
    vlc_mutex_unlock(&state->lock);
}

static
void vout_display_SizeWindow(unsigned *restrict width,
                             unsigned *restrict height,
                             const video_format_t *restrict original,
                             const vlc_rational_t *restrict dar,
                             const struct vout_crop *restrict crop,
                             const struct vout_display_placement *restrict dp)
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

    *width = dp->width;
    *height = dp->height;

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

    if (dp->sar.num > 0 && dp->sar.den > 0)
        w = (w * dp->sar.den) / dp->sar.num;

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
    *width = (w * dp->zoom.num) / dp->zoom.den;
    *height = (h * dp->zoom.num) / dp->zoom.den;
}

void vout_display_ResizeWindow(struct vlc_window *window,
                               const video_format_t *restrict original,
                               const vlc_rational_t *restrict dar,
                               const struct vout_crop *restrict crop,
                               const struct vout_display_placement *restrict dp)
{
    vout_display_window_t *state = window->owner.sys;
    unsigned width, height;

    vlc_mutex_lock(&state->lock);
    video_format_Clean(&state->format);
    video_format_Copy(&state->format, original);
    width = state->display.width;
    height = state->display.height;
    state->display = *dp;
    state->display.width = width;
    state->display.height = height;
    vlc_mutex_unlock(&state->lock);

    vout_display_SizeWindow(&width, &height, original, dar, crop, dp);
    msg_Dbg(window, "requested window size: %ux%u", width, height);
    vlc_window_SetSize(window, width, height);
}

/**
 * Creates a video window, initially without any attached display.
 */
vlc_window_t *vout_display_window_New(vout_thread_t *vout)
{
    vout_display_window_t *state = malloc(sizeof (*state));
    if (state == NULL)
        return NULL;

    video_format_Init(&state->format, 0);
    state->display.width = state->display.height = 0;
    vlc_mutex_init(&state->lock);
    vlc_mouse_Init(&state->mouse.window);
    vlc_mouse_Init(&state->mouse.video);
    state->mouse.last_left_press = INT64_MIN;
    state->mouse.event = NULL;
    state->vout = vout;

    char *modlist = var_InheritString(vout, "window");
    vlc_window_owner_t owner = {
        .cbs = &vout_display_window_cbs,
        .sys = state,
    };
    vlc_window_cfg_t cfg = {
        .is_fullscreen = var_GetBool(&vout->obj, "fullscreen"),
        .is_decorated = var_InheritBool(&vout->obj, "video-deco"),
#if defined(__APPLE__) || defined(_WIN32)
        .x = var_InheritInteger(&vout->obj, "video-x"),
        .y = var_InheritInteger(&vout->obj, "video-y"),
#endif
    };
    vlc_window_t *window;

    var_Create(vout, "window-state", VLC_VAR_INTEGER);
    var_Create(vout, "window-fullscreen", VLC_VAR_BOOL);
    var_Create(vout, "window-fullscreen-output", VLC_VAR_STRING);

    window = vlc_window_New(VLC_OBJECT(vout), modlist, &owner, &cfg);
    free(modlist);
    if (window == NULL)
        free(state);
    return window;
}

/**
 * Destroys a video window.
 * \note The window must be detached.
 */
void vout_display_window_Delete(vlc_window_t *window)
{
    vout_display_window_t *state = window->owner.sys;
    vout_thread_t *vout = state->vout;

    vlc_window_Delete(window);
    var_Destroy(vout, "window-fullscreen-output");
    var_Destroy(vout, "window-fullscreen");
    var_Destroy(vout, "window-state");
    video_format_Clean(&state->format);
    free(state);
}
