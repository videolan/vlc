/*****************************************************************************
 * decoder_device.c: VAAPI decoder_device
 *****************************************************************************
 * Copyright (C) 2017-2019 VLC authors and VideoLAN
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_window.h>
#include <vlc_codec.h>

#include "hw/vaapi/vlc_vaapi.h"
#include <va/va_drmcommon.h>

#ifdef HAVE_VA_WL
# include <va/va_wayland.h>
#endif

#ifdef HAVE_VA_X11
# include <va/va_x11.h>
# include <vlc_xlib.h>
#endif

#ifdef HAVE_VA_DRM
# include <va/va_drm.h>
# include <vlc_fs.h>
# include <fcntl.h>
#endif

#ifdef HAVE_VA_X11
static void
x11_native_destroy_cb(VANativeDisplay native)
{
    XCloseDisplay(native);
}

static struct vlc_vaapi_instance *
x11_init_vaapi_instance(vlc_decoder_device *device, vout_window_t *window,
                        VADisplay *vadpyp)
{
    if (!vlc_xlib_init(VLC_OBJECT(window)))
        return NULL;

    Display *x11dpy = XOpenDisplay(window->display.x11);
    if (x11dpy == NULL)
        return NULL;

    VADisplay vadpy = *vadpyp = vaGetDisplay(x11dpy);
    if (vadpy == NULL)
    {
        x11_native_destroy_cb(x11dpy);
        return NULL;
    }

    return vlc_vaapi_InitializeInstance(VLC_OBJECT(device), vadpy,
                                        x11dpy, x11_native_destroy_cb);
}
#endif

#ifdef HAVE_VA_DRM
static struct vlc_vaapi_instance *
drm_init_vaapi_instance(vlc_decoder_device *device, VADisplay *vadpyp)
{
    return vlc_vaapi_InitializeInstanceDRM(VLC_OBJECT(device), vaGetDisplayDRM,
                                           vadpyp, NULL);
}
#endif

#ifdef HAVE_VA_WL
static struct vlc_vaapi_instance *
wl_init_vaapi_instance(vlc_decoder_device *device, vout_window_t *window,
                       VADisplay *vadpyp)
{
    VADisplay vadpy = *vadpyp = vaGetDisplayWl(window->display.wl);
    if (vadpy == NULL)
        return NULL;

    return vlc_vaapi_InitializeInstance(VLC_OBJECT(device), vadpy, NULL, NULL);
}
#endif

static void
Close(vlc_decoder_device *device)
{
    vlc_vaapi_DestroyInstance(device->sys);
}

static int
Open(vlc_decoder_device *device, vout_window_t *window)
{
    VADisplay vadpy = NULL;
    struct vlc_vaapi_instance *vainst = NULL;
#if defined (HAVE_VA_X11)
    if (window && window->type == VOUT_WINDOW_TYPE_XID)
        vainst = x11_init_vaapi_instance(device, window, &vadpy);
#elif defined(HAVE_VA_WL)
    if (window && window->type == VOUT_WINDOW_TYPE_WAYLAND)
        vainst = wl_init_vaapi_instance(device, window, &vadpy);
#elif defined (HAVE_VA_DRM)
    (void) window;
    vainst = drm_init_vaapi_instance(device, &vadpy);
#else
# error need X11/WL/DRM support
#endif
    if (!vainst)
        return VLC_EGENERIC;
    assert(vadpy != NULL);

    device->sys = vainst;
    device->type = VLC_DECODER_DEVICE_VAAPI;
    device->opaque = vadpy;
    return VLC_SUCCESS;
}

#if defined (HAVE_VA_X11)
# define PRIORITY 2
# define SHORTCUT "vaapi_x11"
# define DESCRIPTION_SUFFIX "X11"
#elif defined(HAVE_VA_WL)
# define PRIORITY 2
# define SHORTCUT "vaapi_wl"
# define DESCRIPTION_SUFFIX "Wayland"
#elif defined (HAVE_VA_DRM)
# define PRIORITY 1
# define SHORTCUT "vaapi_drm"
# define DESCRIPTION_SUFFIX "DRM"
#endif

vlc_module_begin ()
    set_description("VA-API decoder device for " DESCRIPTION_SUFFIX)
    set_capability("decoder device", PRIORITY)
    set_callbacks(Open, Close)
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    add_shortcut("vaapi", SHORTCUT)
vlc_module_end ()
