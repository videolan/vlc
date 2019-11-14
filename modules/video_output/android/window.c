/**
 * @file window.c
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
#include <vlc_codec.h>

#include <dlfcn.h>
#include <jni.h>

#include "utils.h"

static int Open(vout_window_t *);
static void Close(vout_window_t *);
static int OpenDecDevice(vlc_decoder_device *device, vout_window_t *window);

/*
 * Module descriptor
 */
vlc_module_begin()
    set_shortname(N_("Android Window"))
    set_description(N_("Android native window"))
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_capability("vout window", 10)
    set_callback(Open)
    add_submodule ()
        set_callback_dec_device(OpenDecDevice, 1)
        add_shortcut("android")
vlc_module_end()


static void OnNewWindowSize(vout_window_t *wnd,
                            unsigned i_width, unsigned i_height)
{
    vout_window_ReportSize(wnd, i_width, i_height);
}

static void OnNewMouseCoords(vout_window_t *wnd,
                             const struct awh_mouse_coords *coords)
{
    vout_window_ReportMouseMoved(wnd, coords->i_x, coords->i_y);
    switch (coords->i_action)
    {
        case AMOTION_EVENT_ACTION_DOWN:
            vout_window_ReportMousePressed(wnd, coords->i_button);
            break;
        case AMOTION_EVENT_ACTION_UP:
            vout_window_ReportMouseReleased(wnd, coords->i_button);
            break;
        case AMOTION_EVENT_ACTION_MOVE:
            break;
    }
}

static const struct vout_window_operations ops = {
    .destroy = Close,
};

/**
 * Create an Android native window.
 */
static int Open(vout_window_t *wnd)
{
    AWindowHandler *p_awh = AWindowHandler_new(wnd,
        &(awh_events_t) { OnNewWindowSize, OnNewMouseCoords });
    if (p_awh == NULL)
        return VLC_EGENERIC;

    wnd->type = VOUT_WINDOW_TYPE_ANDROID_NATIVE;
    wnd->handle.anativewindow = p_awh;
    wnd->ops = &ops;

    return VLC_SUCCESS;
}


/**
 * Destroys the Android native window.
 */
static void Close(vout_window_t *wnd)
{
    AWindowHandler_destroy(wnd->handle.anativewindow);
}

static int
OpenDecDevice(vlc_decoder_device *device, vout_window_t *window)
{
    if (!window || window->type != VOUT_WINDOW_TYPE_ANDROID_NATIVE)
        return VLC_EGENERIC;

    static const struct vlc_decoder_device_operations ops = 
    {
        .close = NULL,
    };
    device->ops = &ops;
    device->type = VLC_DECODER_DEVICE_AWINDOW;
    device->opaque = window->handle.anativewindow;

    return VLC_SUCCESS;
}
