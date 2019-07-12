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
#include <vlc_fs.h>

#include "vlc_vaapi.h"
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

typedef void (*vaapi_native_destroy_cb)(VANativeDisplay);
struct vaapi_instance;

struct vaapi_instance
{
    VADisplay dpy;
    VANativeDisplay native;
    vaapi_native_destroy_cb native_destroy_cb;
};

/* Initializes the VADisplay. If not NULL, native_destroy_cb will be called
 * when the instance is released in order to destroy the native holder (that
 * can be a drm/x11/wl). On error, dpy is terminated and the destroy callback
 * is called. */
static struct vaapi_instance *
vaapi_InitializeInstance(vlc_object_t *o, VADisplay dpy,
                             VANativeDisplay native,
                             vaapi_native_destroy_cb native_destroy_cb)
{
    int major = 0, minor = 0;
    VAStatus s = vaInitialize(dpy, &major, &minor);
    if (s != VA_STATUS_SUCCESS)
    {
        msg_Err(o, "vaInitialize: %s", vaErrorStr(s));
        goto error;
    }
    struct vaapi_instance *inst = malloc(sizeof(*inst));

    if (unlikely(inst == NULL))
        goto error;
    inst->dpy = dpy;
    inst->native = native;
    inst->native_destroy_cb = native_destroy_cb;

    return inst;
error:
    vaTerminate(dpy);
    if (native != NULL && native_destroy_cb != NULL)
        native_destroy_cb(native);
    return NULL;
}

static void
vaapi_DestroyInstance(struct vaapi_instance *inst)
{
    vaTerminate(inst->dpy);
    if (inst->native != NULL && inst->native_destroy_cb != NULL)
        inst->native_destroy_cb(inst->native);
    free(inst);
}

#ifdef HAVE_VA_X11
static void
x11_native_destroy_cb(VANativeDisplay native)
{
    XCloseDisplay(native);
}

static struct vaapi_instance *
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

    return vaapi_InitializeInstance(VLC_OBJECT(device), vadpy,
                                    x11dpy, x11_native_destroy_cb);
}
#endif

#ifdef HAVE_VA_DRM

static void
native_drm_destroy_cb(VANativeDisplay native)
{
    vlc_close((intptr_t) native);
}

/* Get and Initializes a VADisplay from a DRM device. If device is NULL, this
 * function will try to open default devices. */
static struct vaapi_instance *
vaapi_InitializeInstanceDRM(vlc_object_t *o,
                            VADisplay (*pf_getDisplayDRM)(int),
                            VADisplay *pdpy, const char *device)
{
    static const char *default_drm_device_paths[] = {
        "/dev/dri/renderD128",
        "/dev/dri/card0",
        "/dev/dri/renderD129",
        "/dev/dri/card1",
    };

    const char *user_drm_device_paths[] = { device };
    const char **drm_device_paths;
    size_t drm_device_paths_count;

    if (device != NULL)
    {
        drm_device_paths = user_drm_device_paths;
        drm_device_paths_count = 1;
    }
    else
    {
        drm_device_paths = default_drm_device_paths;
        drm_device_paths_count = ARRAY_SIZE(default_drm_device_paths);
    }

    for (size_t i = 0; i < drm_device_paths_count; i++)
    {
        int drm_fd = vlc_open(drm_device_paths[i], O_RDWR);
        if (drm_fd < 0)
            continue;

        VADisplay dpy = pf_getDisplayDRM(drm_fd);
        if (dpy)
        {
            struct vaapi_instance *va_inst =
                vaapi_InitializeInstance(o, dpy,
                                         (VANativeDisplay)(intptr_t)drm_fd,
                                         native_drm_destroy_cb);
            if (va_inst)
            {
                *pdpy = dpy;
                return va_inst;
            }
        }
        else
            vlc_close(drm_fd);
    }
    return NULL;
}

static struct vaapi_instance *
drm_init_vaapi_instance(vlc_decoder_device *device, VADisplay *vadpyp)
{
    return vaapi_InitializeInstanceDRM(VLC_OBJECT(device), vaGetDisplayDRM,
                                       vadpyp, NULL);
}
#endif

#ifdef HAVE_VA_WL
static struct vaapi_instance *
wl_init_vaapi_instance(vlc_decoder_device *device, vout_window_t *window,
                       VADisplay *vadpyp)
{
    VADisplay vadpy = *vadpyp = vaGetDisplayWl(window->display.wl);
    if (vadpy == NULL)
        return NULL;

    return vaapi_InitializeInstance(VLC_OBJECT(device), vadpy, NULL, NULL);
}
#endif

static void
Close(vlc_decoder_device *device)
{
    vaapi_DestroyInstance(device->sys);
}

static const struct vlc_decoder_device_operations ops = {
    .close = Close,
};

static int
Open(vlc_decoder_device *device, vout_window_t *window)
{
    VADisplay vadpy = NULL;
    struct vaapi_instance *vainst = NULL;
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

    device->ops = &ops;
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
    set_callback_dec_device(Open, PRIORITY)
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    add_shortcut("vaapi", SHORTCUT)
vlc_module_end ()
