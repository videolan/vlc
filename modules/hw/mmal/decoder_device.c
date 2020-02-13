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

#include <bcm_host.h>
#include <interface/vcsm/user-vcsm.h>

#include "mmal_picture.h"

static int OpenDecoderDevice(vlc_decoder_device *, vout_window_t *);

vlc_module_begin()
    set_description(N_("MMAL"))
    set_callback_dec_device(OpenDecoderDevice, 100)
    add_shortcut("mmal")
vlc_module_end()


typedef enum {
    VCSM_INIT_NONE = 0,
    VCSM_INIT_LEGACY,
    VCSM_INIT_CMA
} vcsm_init_type_t;

// Preferred mode - none->cma on Pi4 otherwise legacy
static volatile vcsm_init_type_t last_vcsm_type = VCSM_INIT_NONE;

static vcsm_init_type_t cma_vcsm_init(void)
{
    vcsm_init_type_t rv = VCSM_INIT_NONE;
    // We don't bother locking - taking a copy here should be good enough
    vcsm_init_type_t try_type = last_vcsm_type;

    if (try_type == VCSM_INIT_NONE) {
        if (bcm_host_is_fkms_active())
            try_type = VCSM_INIT_CMA;
        else
            try_type = VCSM_INIT_LEGACY;
    }

    if (try_type == VCSM_INIT_CMA) {
        if (vcsm_init_ex(1, -1) == 0)
            rv = VCSM_INIT_CMA;
        else if (vcsm_init_ex(0, -1) == 0)
            rv = VCSM_INIT_LEGACY;
    }
    else
    {
        if (vcsm_init_ex(0, -1) == 0)
            rv = VCSM_INIT_LEGACY;
        else if (vcsm_init_ex(1, -1) == 0)
            rv = VCSM_INIT_CMA;
    }

    // Just in case this affects vcsm init do after that
    if (rv != VCSM_INIT_NONE)
        bcm_host_init();

    last_vcsm_type = rv;
    return rv;
}

static void cma_vcsm_exit()
{
    vcsm_exit();
    bcm_host_deinit();  // Does nothing but add in case it ever does
}

static const char * cma_vcsm_init_str(const vcsm_init_type_t init_mode)
{
    switch (init_mode)
    {
        case VCSM_INIT_CMA:
            return "CMA";
        case VCSM_INIT_LEGACY:
            return "Legacy";
        default:
            vlc_assert_unreachable();
            return NULL;
    }
}

static void CloseDecoderDevice(vlc_decoder_device *device)
{
    VLC_UNUSED(device);
    cma_vcsm_exit();
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

    sys->is_cma = vcsm_init_type == VCSM_INIT_CMA;

    device->ops = &mmal_device_ops;
    device->opaque = sys;
    device->type = VLC_DECODER_DEVICE_MMAL;
    device->sys = sys;

    msg_Warn(device, "VCSM init succeeded: %s", cma_vcsm_init_str(vcsm_init_type));

    return VLC_SUCCESS;
}
