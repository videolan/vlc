/*****************************************************************************
 * decoder_device.c: Decoder device and video context
 *****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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
#include <vlc_codec.h>
#include <vlc_modules.h>
#include "libvlc.h"

struct vlc_decoder_device_priv
{
    struct vlc_decoder_device device;
    vlc_atomic_rc_t rc;
};

static int decoder_device_Open(void *func, bool forced, va_list ap)
{
    VLC_UNUSED(forced);
    vlc_decoder_device_Open open = func;
    vlc_decoder_device *device = va_arg(ap, vlc_decoder_device *);
    vlc_window_t *window = va_arg(ap, vlc_window_t *);
    int ret = open(device, window);
    if (ret != VLC_SUCCESS)
        vlc_objres_clear(&device->obj);
    return ret;
}

vlc_decoder_device *
vlc_decoder_device_Create(vlc_object_t *o, vlc_window_t *window)
{
    struct vlc_decoder_device_priv *priv =
            vlc_custom_create(o, sizeof (*priv), "decoder device");
    if (!priv)
        return NULL;
    char *name = var_InheritString(o, "dec-dev");
    module_t *module = vlc_module_load(&priv->device, "decoder device", name,
                                    true, decoder_device_Open, &priv->device,
                                    window);
    free(name);
    if (module == NULL)
    {
        vlc_objres_clear(VLC_OBJECT(&priv->device));
        vlc_object_delete(&priv->device);
        return NULL;
    }
    assert(priv->device.ops != NULL);
    vlc_atomic_rc_init(&priv->rc);
    return &priv->device;
}

vlc_decoder_device *
vlc_decoder_device_Hold(vlc_decoder_device *device)
{
    struct vlc_decoder_device_priv *priv =
            container_of(device, struct vlc_decoder_device_priv, device);
    vlc_atomic_rc_inc(&priv->rc);
    return device;
}

void
vlc_decoder_device_Release(vlc_decoder_device *device)
{
    struct vlc_decoder_device_priv *priv =
            container_of(device, struct vlc_decoder_device_priv, device);
    if (vlc_atomic_rc_dec(&priv->rc))
    {
        if (device->ops->close != NULL)
            device->ops->close(device);
        vlc_objres_clear(VLC_OBJECT(device));
        vlc_object_delete(device);
    }
}

/* video context */

struct vlc_video_context
{
    vlc_atomic_rc_t    rc;
    vlc_decoder_device *device;
    const struct vlc_video_context_operations *ops;
    enum vlc_video_context_type private_type;
    size_t private_size;
    uint8_t private[];
};

vlc_video_context * vlc_video_context_Create(vlc_decoder_device *device,
                                          enum vlc_video_context_type private_type,
                                          size_t private_size,
                                          const struct vlc_video_context_operations *ops)
{
    assert(private_type != 0);
    vlc_video_context *vctx = malloc(sizeof(*vctx) + private_size);
    if (unlikely(vctx == NULL))
        return NULL;
    vlc_atomic_rc_init( &vctx->rc );
    vctx->private_type = private_type;
    vctx->private_size = private_size;
    vctx->device = device;
    if (vctx->device)
        vlc_decoder_device_Hold( vctx->device );
    vctx->ops = ops;
    return vctx;
}

void *vlc_video_context_GetPrivate(vlc_video_context *vctx, enum vlc_video_context_type type)
{
    if (vctx && vctx->private_type == type)
        return &vctx->private;
    return NULL;
}

enum vlc_video_context_type vlc_video_context_GetType(const vlc_video_context *vctx)
{
    return vctx->private_type;
}

vlc_video_context *vlc_video_context_Hold(vlc_video_context *vctx)
{
    vlc_atomic_rc_inc( &vctx->rc );
    return vctx;
}

void vlc_video_context_Release(vlc_video_context *vctx)
{
    if ( vlc_atomic_rc_dec( &vctx->rc ) )
    {
        if (vctx->device)
            vlc_decoder_device_Release( vctx->device );
        if ( vctx->ops && vctx->ops->destroy )
            vctx->ops->destroy( vlc_video_context_GetPrivate(vctx, vctx->private_type) );
        free(vctx);
    }
}

vlc_decoder_device* vlc_video_context_HoldDevice(vlc_video_context *vctx)
{
    if (!vctx->device)
        return NULL;
    return vlc_decoder_device_Hold( vctx->device );
}
