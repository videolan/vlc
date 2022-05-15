/*****************************************************************************
 * device.c: VDPAU instance management for VLC
 *****************************************************************************
 * Copyright (C) 2013-2021 Rémi Denis-Courmont
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

#include <errno.h>
#include <stdlib.h>

#include <vlc_common.h>
#include <vlc_codec.h>
#include <vlc_plugin.h>
#include <vlc_window.h>
#include <vlc_xlib.h>
#include "vlc_vdpau.h"

typedef struct vdp_instance
{
    Display *display;
    struct vlc_vdp_device device;
} vdp_instance_t;

static void Close(vlc_decoder_device *device)
{
    vdp_instance_t *vi = device->sys;

    vdp_device_destroy(vi->device.vdp, vi->device.device);
    vdp_destroy_x11(vi->device.vdp);
    XCloseDisplay(vi->display);
    free(vi);
}

static const struct vlc_decoder_device_operations ops = {
    .close = Close,
};

static int ScreenNumberOfWindow(Display *dpy, Window w)
{
    XWindowAttributes attrs;

    XGetWindowAttributes(dpy, w, &attrs);

    for (int num = 0; num < ScreenCount(dpy); num++)
        if (RootWindow(dpy, num) == attrs.root)
            return num;

    return -1;
}

static int Open(vlc_decoder_device *device, vlc_window_t *window)
{
    int errCode = VLC_EGENERIC;

    if (window == NULL || window->type != VLC_WINDOW_TYPE_XID)
        return VLC_ENOTSUP;
    if (!vlc_xlib_init(VLC_OBJECT(device)))
        return VLC_ENOTSUP;

    vdp_instance_t *vi = malloc(sizeof (*vi));
    if (unlikely(vi == NULL))
        return VLC_ENOMEM;

    vi->display = XOpenDisplay(window->display.x11);
    if (vi->display == NULL)
    {
        free(vi);
        return -ENOBUFS;
    }

    int num = ScreenNumberOfWindow(vi->display, window->handle.xid);
    if (unlikely(num < 0))
        goto error;

    VdpStatus err = vdp_create_x11(vi->display, num, &vi->device.vdp,
                                   &vi->device.device);
    if (err != VDP_STATUS_OK)
        goto error;

    const char *infos;
    err = vdp_get_information_string(vi->device.vdp, &infos);
    if (err == VDP_STATUS_OK)
    {
        if (strstr(infos, "VAAPI") != NULL)
        {
            vdp_device_destroy(vi->device.vdp, vi->device.device);
            vdp_destroy_x11(vi->device.vdp);
            errCode = -EACCES;
            goto error;
        }

        msg_Info(device, "Using %s", infos);
    }

    device->ops = &ops;
    device->sys = vi;
    device->type = VLC_DECODER_DEVICE_VDPAU;
    device->opaque = &vi->device;
    return VLC_SUCCESS;

error:
    XCloseDisplay(vi->display);
    free(vi);
    return errCode;
}

vlc_module_begin()
    set_description("VDPAU decoder device")
    set_callback_dec_device(Open, 3)
    add_shortcut("vdpau")
    set_subcategory(SUBCAT_VIDEO_VOUT)
vlc_module_end()
