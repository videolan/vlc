/**
 * @file androidnativewindow.c
 * @brief Android native window provider module for VLC media player
 */
/*****************************************************************************
 * Copyright © 2013 VLC authors and VideoLAN
 *
 * Author: Adrien Maglo <magsoft@videolan.org>
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

#include <stdarg.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_window.h>

#include <dlfcn.h>
#include <jni.h>

#include "utils.h"

#define THREAD_NAME "ANativeWindow"

static int Open(vout_window_t *, const vout_window_cfg_t *);
static void Close(vout_window_t *);
static int Control(vout_window_t *, int, va_list ap);

/*
 * Module descriptor
 */
vlc_module_begin()
    set_shortname(N_("ANativeWindow"))
    set_description(N_("Android native window"))
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_capability("vout window", 0)
    set_callbacks(Open, Close)
vlc_module_end()


struct vout_window_sys_t
{
    AWindowHandler *p_awh;
};

/**
 * Create an Android native window.
 */
static int Open(vout_window_t *wnd, const vout_window_cfg_t *cfg)
{
    ANativeWindow *p_anw;

    if (cfg->type != VOUT_WINDOW_TYPE_INVALID
     && cfg->type != VOUT_WINDOW_TYPE_ANDROID_NATIVE)
        return VLC_EGENERIC;

    vout_window_sys_t *p_sys = malloc(sizeof (*p_sys));
    if (p_sys == NULL)
        return VLC_ENOMEM;

    p_sys->p_awh = AWindowHandler_new(VLC_OBJECT(wnd));
    if (!p_sys->p_awh)
        goto error;
    p_anw = AWindowHandler_getANativeWindow(p_sys->p_awh, AWindow_Video);
    if (!p_anw)
        goto error;

    wnd->type = VOUT_WINDOW_TYPE_ANDROID_NATIVE;
    wnd->handle.anativewindow = p_anw;
    wnd->control = Control;
    wnd->sys = p_sys;

    // Set the Java surface size.
    AWindowHandler_setWindowLayout(p_sys->p_awh, cfg->width, cfg->height,
                                   cfg->width, cfg->height, 1, 1);

    return VLC_SUCCESS;

error:
    if (p_sys->p_awh)
        AWindowHandler_destroy(p_sys->p_awh);
    free(p_sys);
    return VLC_EGENERIC;
}


/**
 * Destroys the Android native window.
 */
static void Close(vout_window_t *wnd)
{
    vout_window_sys_t *p_sys = wnd->sys;
    AWindowHandler_destroy(p_sys->p_awh);
    free (p_sys);
}


/**
 * Window control.
 */
static int Control(vout_window_t *wnd, int cmd, va_list ap)
{
    switch (cmd)
    {
        case VOUT_WINDOW_SET_SIZE:
        {
            unsigned width = va_arg(ap, unsigned);
            unsigned height = va_arg(ap, unsigned);
            AWindowHandler_setWindowLayout(wnd->sys->p_awh, width, height,
                                           width, height, 1, 1);
            break;
        }
        case VOUT_WINDOW_SET_STATE:
        case VOUT_WINDOW_SET_FULLSCREEN:
            return VLC_EGENERIC;
        default:
            msg_Err (wnd, "request %d not implemented", cmd);
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}
