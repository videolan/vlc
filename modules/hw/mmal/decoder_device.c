/*****************************************************************************
 * decoder_device.c: MMAL-based decoder plugin for Raspberry Pi
 *****************************************************************************
 * Copyright © 2020 Steve Lhomme
 *
 * Authors: Steve Lhomme <robux4@videolabs.io>
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
#include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_window.h>
#include <vlc_codec.h>

#include "mmal_picture.h"

static int OpenDecoderDevice(vlc_decoder_device *, vout_window_t *);

vlc_module_begin()
    set_description(N_("MMAL"))
    set_callback_dec_device(OpenDecoderDevice, 100)
    add_shortcut("mmal-device")
vlc_module_end()

static void CloseDecoderDevice(vlc_decoder_device *device)
{
    mmal_decoder_device_t *sys = device->opaque;
    cma_vcsm_exit(sys->vcsm_init_type);
}

static const struct vlc_decoder_device_operations mmal_device_ops = {
    .close = CloseDecoderDevice,
};

static int OpenDecoderDevice(vlc_decoder_device *device, vout_window_t *window)
{
    VLC_UNUSED(window);

    mmal_decoder_device_t *sys = vlc_obj_malloc(VLC_OBJECT(device), sizeof(mmal_decoder_device_t));
    if (unlikely(sys==NULL))
        return VLC_ENOMEM;

    vcsm_init_type_t vcsm_init_type = cma_vcsm_init();
    if (vcsm_init_type == VCSM_INIT_NONE) {
        msg_Err(device, "VCSM init failed");
        return VLC_EGENERIC;
    }

    sys->vcsm_init_type = vcsm_init_type;

    device->ops = &mmal_device_ops;
    device->opaque = sys;
    device->type = VLC_DECODER_DEVICE_MMAL;
    device->sys = sys;

    msg_Warn(device, "VCSM init succeeded: %s", cma_vcsm_init_str(vcsm_init_type));

    return VLC_SUCCESS;
}
