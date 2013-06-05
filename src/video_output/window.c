/*****************************************************************************
 * window.c: "vout window" managment
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

vout_window_t *vout_window_New(vlc_object_t *obj,
                               const char *module,
                               const vout_window_cfg_t *cfg)
{
    window_t *w = vlc_custom_create(obj, sizeof(*w), "window");
    vout_window_t *window = &w->wnd;

    memset(&window->handle, 0, sizeof(window->handle));
    window->control = NULL;
    window->sys = NULL;

    const char *type;
    switch (cfg->type) {
#if defined(_WIN32) || defined(__OS2__)
    case VOUT_WINDOW_TYPE_HWND:
        type = "vout window hwnd";
        window->handle.hwnd = NULL;
        break;
#endif
#ifdef __APPLE__
    case VOUT_WINDOW_TYPE_NSOBJECT:
        type = "vout window nsobject";
        window->handle.nsobject = NULL;
        break;
#endif
    case VOUT_WINDOW_TYPE_XID:
        type = "vout window xid";
        window->handle.xid = 0;
        window->display.x11 = NULL;
        break;
    default:
        assert(0);
    }

    w->module = vlc_module_load(window, type, module, module && *module,
                                vout_window_start, window, cfg);
    if (!w->module) {
        vlc_object_release(window);
        return NULL;
    }

    /* Hook for screensaver inhibition */
    if (var_InheritBool(obj, "disable-screensaver") &&
        cfg->type == VOUT_WINDOW_TYPE_XID) {
        w->inhibit = vlc_inhibit_Create(VLC_OBJECT (window));
        if (w->inhibit != NULL)
            vlc_inhibit_Set(w->inhibit, VLC_INHIBIT_VIDEO);
            /* FIXME: ^ wait for vout activation, pause */
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

    vlc_module_unload(w->module, vout_window_stop, window);
    vlc_object_release(window);
}

int vout_window_Control(vout_window_t *window, int query, ...)
{
    va_list args;
    va_start(args, query);
    int ret = window->control(window, query, args);
    va_end(args);

    return ret;
}

