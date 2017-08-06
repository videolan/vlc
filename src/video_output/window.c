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
    window->control = NULL;
    window->sys = NULL;

    if (owner != NULL)
        window->owner = *owner;
    else
        window->owner.resized = NULL;

    w->module = vlc_module_load(window, "vout window", module,
                                module && *module,
                                vout_window_start, window, cfg);
    if (!w->module) {
        vlc_object_release(window);
        return NULL;
    }

    /* Hook for screensaver inhibition */
    if (var_InheritBool(obj, "disable-screensaver") &&
        window->type == VOUT_WINDOW_TYPE_XID) {
        w->inhibit = vlc_inhibit_Create(VLC_OBJECT (window));
        if (w->inhibit != NULL)
            vlc_inhibit_Set(w->inhibit, VLC_INHIBIT_VIDEO);
    }
    else
        w->inhibit = NULL;
    return window;
}

static void vout_window_stop(void *func, va_list ap)
{
    int (*deactivate)(vout_window_t *) = func;
    vout_window_t *wnd = va_arg(ap, vout_window_t *);

    deactivate(wnd);
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

    vlc_module_unload(window, w->module, vout_window_stop, window);
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
#include "event.h"

typedef struct vout_display_window
{
    vout_display_t *vd;
    unsigned width;
    unsigned height;

    vlc_mutex_t lock;
} vout_display_window_t;

static void vout_display_window_ResizeNotify(vout_window_t *window,
                                             unsigned width, unsigned height)
{
    vout_display_window_t *state = window->owner.sys;

    msg_Dbg(window, "resized to %ux%u", width, height);
    vlc_mutex_lock(&state->lock);
    state->width = width;
    state->height = height;

    if (state->vd != NULL)
        vout_display_SendEventDisplaySize(state->vd, width, height);
    vlc_mutex_unlock(&state->lock);
}

static void vout_display_window_CloseNotify(vout_window_t *window)
{
    vout_thread_t *vout = (vout_thread_t *)window->obj.parent;

    vout_SendEventClose(vout);
}

static void vout_display_window_MouseEvent(vout_window_t *window,
                                           const vout_window_mouse_event_t *mouse)
{
    vout_thread_t *vout = (vout_thread_t *)window->obj.parent;
    vout_WindowMouseEvent(vout, mouse);
}

/**
 * Creates a video window, initially without any attached display.
 */
vout_window_t *vout_display_window_New(vout_thread_t *vout,
                                       const vout_window_cfg_t *cfg)
{
    vout_display_window_t *state = malloc(sizeof (*state));
    if (state == NULL)
        return NULL;

    state->vd = NULL;
    state->width = cfg->width;
    state->height = cfg->height;
    vlc_mutex_init(&state->lock);

    vout_window_owner_t owner = {
        .sys = state,
        .resized = vout_display_window_ResizeNotify,
        .closed = vout_display_window_CloseNotify,
        .mouse_event = vout_display_window_MouseEvent,
    };
    vout_window_t *window;

    window = vout_window_New((vlc_object_t *)vout, "$window", cfg, &owner);
    if (window == NULL) {
        vlc_mutex_destroy(&state->lock);
        free(state);
    }
    return window;
}

/**
 * Attaches a window to a display. Window events will be dispatched to the
 * display until they are detached.
 */
void vout_display_window_Attach(vout_window_t *window, vout_display_t *vd)
{
    vout_display_window_t *state = window->owner.sys;

    vout_window_SetSize(window,
                        vd->cfg->display.width, vd->cfg->display.height);

    vlc_mutex_lock(&state->lock);
    state->vd = vd;

    vout_display_SendEventDisplaySize(vd, state->width, state->height);
    vlc_mutex_unlock(&state->lock);
}

/**
 * Detaches a window from a display. Window events will no longer be dispatched
 * (except those that do not need a display).
 */
void vout_display_window_Detach(vout_window_t *window)
{
    vout_display_window_t *state = window->owner.sys;

    vlc_mutex_lock(&state->lock);
    state->vd = NULL;
    vlc_mutex_unlock(&state->lock);
}

/**
 * Destroys a video window.
 * \note The window must be detached.
 */
void vout_display_window_Delete(vout_window_t *window)
{
    vout_display_window_t *state = window->owner.sys;

    vout_window_Delete(window);

    assert(state->vd == NULL);
    vlc_mutex_destroy(&state->lock);
    free(state);
}
