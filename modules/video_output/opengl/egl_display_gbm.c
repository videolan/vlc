/*****************************************************************************
 * egl_display_gbm.c
 *****************************************************************************
 * Copyright (C) 2021 Videolabs
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
#include <vlc_opengl.h>
#include <vlc_fs.h>
#include <fcntl.h>
#include <assert.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <gbm.h>

#include "egl_display.h"

struct sys
{
    struct gbm_device *device;
    int fd;
};

static void Close(struct vlc_egl_display *display)
{
    struct sys *sys = display->sys;

    assert(sys->device);
    assert(sys->fd >= 0);

    gbm_device_destroy(sys->device);
    vlc_close(sys->fd);

    free(sys);
}

static int
OpenDeviceFd(const char **out_path)
{
    static const char *default_drm_device_paths[] = {
        "/dev/dri/renderD128",
        "/dev/dri/card0",
        "/dev/dri/renderD129",
        "/dev/dri/card1",
    };

    for (size_t i = 0; i < ARRAY_SIZE(default_drm_device_paths); ++i)
    {
        const char *path = default_drm_device_paths[i];
        int fd = vlc_open(path, O_RDWR);
        if (fd >= 0)
        {
            *out_path = path;
            return fd;
        }
    }

    return -1;
}

static vlc_egl_display_open_fn Open;
static int
Open(struct vlc_egl_display *display)
{
    struct sys *sys = display->sys = malloc(sizeof(*sys));
    if (!sys)
        return VLC_EGENERIC;

    const char *extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    if (!extensions)
        return VLC_EGENERIC;

    if (!vlc_gl_StrHasToken(extensions, "EGL_KHR_platform_gbm"))
        return VLC_EGENERIC;

    const char *device_path;
    sys->fd = OpenDeviceFd(&device_path);
    if (sys->fd < 0)
        return VLC_EGENERIC;

    sys->device = gbm_create_device(sys->fd);
    if (!sys->device)
    {
        vlc_close(sys->fd);
        return VLC_EGENERIC;
    }

    display->display = eglGetPlatformDisplay(EGL_PLATFORM_GBM_KHR, sys->device,
                                             NULL);
    if (display->display == EGL_NO_DISPLAY)
    {
        gbm_device_destroy(sys->device);
        vlc_close(sys->fd);
        return VLC_EGENERIC;
    }

    static const struct vlc_egl_display_ops ops = {
        .close = Close,
    };
    display->ops = &ops;

    msg_Info(display, "EGL using GBM platform on device %s (fd=%d)",
                      device_path, sys->fd);
    return VLC_SUCCESS;
}

vlc_module_begin()
    set_description("EGL GBM display")
    set_capability("egl display", 2)
    set_callback(Open)
    add_shortcut("egl_display_gbm")
vlc_module_end()
