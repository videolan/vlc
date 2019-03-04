/*****************************************************************************
 * decoder_helpers.c: Functions for the management of decoders
 *****************************************************************************
 * Copyright (C) 1999-2019 VLC authors and VideoLAN
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Laurent Aimar <fenrir@via.ecp.fr>
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
#include <vlc_atomic.h>
#include <vlc_meta.h>
#include <vlc_modules.h>

void decoder_Init( decoder_t *p_dec, const es_format_t *restrict p_fmt )
{
    p_dec->i_extra_picture_buffers = 0;
    p_dec->b_frame_drop_allowed = false;

    p_dec->pf_decode = NULL;
    p_dec->pf_get_cc = NULL;
    p_dec->pf_packetize = NULL;
    p_dec->pf_flush = NULL;
    p_dec->p_module = NULL;

    es_format_Copy( &p_dec->fmt_in, p_fmt );
    es_format_Init( &p_dec->fmt_out, p_fmt->i_cat, 0 );
}

void decoder_Clean( decoder_t *p_dec )
{
    if ( p_dec->p_module != NULL )
    {
        module_unneed(p_dec, p_dec->p_module);
        p_dec->p_module = NULL;
    }

    es_format_Clean( &p_dec->fmt_in );
    es_format_Clean( &p_dec->fmt_out );

    if ( p_dec->p_description )
    {
        vlc_meta_Delete(p_dec->p_description);
        p_dec->p_description = NULL;
    }
}

void decoder_Destroy( decoder_t *p_dec )
{
    if (p_dec != NULL)
    {
        decoder_Clean( p_dec );
        vlc_object_release( p_dec );
    }
}

int decoder_UpdateVideoFormat( decoder_t *dec )
{
    vlc_assert( dec->fmt_in.i_cat == VIDEO_ES && dec->cbs != NULL );
    if ( unlikely(dec->fmt_in.i_cat != VIDEO_ES || dec->cbs == NULL ||
                  dec->cbs->video.format_update == NULL) )
        return -1;

    return dec->cbs->video.format_update( dec );
}

picture_t *decoder_NewPicture( decoder_t *dec )
{
    vlc_assert( dec->fmt_in.i_cat == VIDEO_ES && dec->cbs != NULL );
    return dec->cbs->video.buffer_new( dec );
}

struct vlc_decoder_device_priv
{
    struct vlc_decoder_device device;
    vlc_atomic_rc_t rc;
    module_t *module;
};

static int decoder_device_Open(void *func, bool forced, va_list ap)
{
    vlc_decoder_device_Open open = func;
    vlc_decoder_device *device = va_arg(ap, vlc_decoder_device *);
    vout_window_t *window = va_arg(ap, vout_window_t *);
    int ret = open(device, window);
    if (ret != VLC_SUCCESS)
    {
        device->sys = NULL;
        device->type = VLC_DECODER_DEVICE_NONE;
        device->opaque = NULL;
    }
    else
    {
        assert(device->type != VLC_DECODER_DEVICE_NONE);
    }
    (void) forced;
    return ret;
}

static void decoder_device_Close(void *func, va_list ap)
{
    vlc_decoder_device_Close close = func;
    vlc_decoder_device *device = va_arg(ap, vlc_decoder_device *);
    close(device);
}

vlc_decoder_device *
vlc_decoder_device_Create(vout_window_t *window)
{
    struct vlc_decoder_device_priv *priv =
            vlc_object_create(window, sizeof (*priv));
    if (!priv)
        return NULL;
    char *name = var_InheritString(window, "dec-dev");
    priv->module = vlc_module_load(&priv->device, "decoder device", name,
                                    true, decoder_device_Open, &priv->device,
                                    window);
    free(name);
    if (!priv->module)
    {
        vlc_object_release(&priv->device);
        return NULL;
    }
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
        vlc_module_unload(device, priv->module, decoder_device_Close,
                          device);
        vlc_object_release(device);
    }
}
