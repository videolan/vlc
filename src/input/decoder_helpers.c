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
#include <vlc_picture.h>
#include "libvlc.h"

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
        vlc_object_delete(p_dec);
    }
}

int decoder_UpdateVideoFormat( decoder_t *dec )
{
    return decoder_UpdateVideoOutput( dec, NULL );
}

int decoder_UpdateVideoOutput( decoder_t *dec, vlc_video_context *vctx_out )
{
    vlc_assert( dec->fmt_in.i_cat == VIDEO_ES && dec->cbs != NULL );
    if ( unlikely(dec->fmt_in.i_cat != VIDEO_ES || dec->cbs == NULL) )
        return -1;

    /* */
    dec->fmt_out.video.i_chroma = dec->fmt_out.i_codec;

    if( dec->fmt_out.video.i_visible_height == 1088 &&
        var_CreateGetBool( dec, "hdtv-fix" ) )
    {
        dec->fmt_out.video.i_visible_height = 1080;
        if( !(dec->fmt_out.video.i_sar_num % 136))
        {
            dec->fmt_out.video.i_sar_num *= 135;
            dec->fmt_out.video.i_sar_den *= 136;
        }
        msg_Warn( dec, "Fixing broken HDTV stream (display_height=1088)");
    }

    if( !dec->fmt_out.video.i_sar_num || !dec->fmt_out.video.i_sar_den )
    {
        dec->fmt_out.video.i_sar_num = 1;
        dec->fmt_out.video.i_sar_den = 1;
    }

    vlc_ureduce( &dec->fmt_out.video.i_sar_num, &dec->fmt_out.video.i_sar_den,
                    dec->fmt_out.video.i_sar_num, dec->fmt_out.video.i_sar_den, 50000 );

    if( vlc_fourcc_IsYUV( dec->fmt_out.video.i_chroma ) )
    {
        const vlc_chroma_description_t *dsc = vlc_fourcc_GetChromaDescription( dec->fmt_out.video.i_chroma );
        for( unsigned int i = 0; dsc && i < dsc->plane_count; i++ )
        {
            while( dec->fmt_out.video.i_width % dsc->p[i].w.den )
                dec->fmt_out.video.i_width++;
            while( dec->fmt_out.video.i_height % dsc->p[i].h.den )
                dec->fmt_out.video.i_height++;
        }
    }

    if( !dec->fmt_out.video.i_visible_width ||
        !dec->fmt_out.video.i_visible_height )
    {
        if( dec->fmt_in.video.i_visible_width &&
            dec->fmt_in.video.i_visible_height )
        {
            dec->fmt_out.video.i_visible_width  = dec->fmt_in.video.i_visible_width;
            dec->fmt_out.video.i_visible_height = dec->fmt_in.video.i_visible_height;
            dec->fmt_out.video.i_x_offset       = dec->fmt_in.video.i_x_offset;
            dec->fmt_out.video.i_y_offset       = dec->fmt_in.video.i_y_offset;
        }
        else
        {
            dec->fmt_out.video.i_visible_width  = dec->fmt_out.video.i_width;
            dec->fmt_out.video.i_visible_height = dec->fmt_out.video.i_height;
            dec->fmt_out.video.i_x_offset       = 0;
            dec->fmt_out.video.i_y_offset       = 0;
        }
    }

    video_format_AdjustColorSpace( &dec->fmt_out.video );

    if (dec->cbs->video.format_update == NULL)
        return 0;

    return dec->cbs->video.format_update( dec, vctx_out );
}

picture_t *decoder_NewPicture( decoder_t *dec )
{
    vlc_assert( dec->fmt_in.i_cat == VIDEO_ES && dec->cbs != NULL );
    if (dec->cbs->video.buffer_new == NULL)
        return picture_NewFromFormat( &dec->fmt_out.video );
    return dec->cbs->video.buffer_new( dec );
}

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
    vout_window_t *window = va_arg(ap, vout_window_t *);
    return open(device, window);
}

vlc_decoder_device *
vlc_decoder_device_Create(vlc_object_t *o, vout_window_t *window)
{
    struct vlc_decoder_device_priv *priv =
            vlc_object_create(o, sizeof (*priv));
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
    assert(private_type != VLC_VIDEO_CONTEXT_NONE); // use a NULL video context for that
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


/** encoder **/
vlc_decoder_device *vlc_encoder_GetDecoderDevice( encoder_t *enc )
{
    vlc_assert( enc->fmt_in.i_cat == VIDEO_ES && enc->cbs != NULL );
    if ( unlikely(enc->fmt_in.i_cat != VIDEO_ES || enc->cbs == NULL ) )
        return NULL;

    return enc->cbs->video.get_device( enc );
}
