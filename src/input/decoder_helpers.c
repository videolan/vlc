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

void decoder_Init( decoder_t *p_dec, es_format_t *restrict fmt_in, const es_format_t *restrict p_fmt )
{
    p_dec->i_extra_picture_buffers = 0;
    p_dec->b_frame_drop_allowed = false;

    p_dec->pf_decode = NULL;
    p_dec->pf_get_cc = NULL;
    p_dec->pf_packetize = NULL;
    p_dec->pf_flush = NULL;
    p_dec->p_module = NULL;

    assert(fmt_in != NULL);
    es_format_Copy( fmt_in, p_fmt );
    p_dec->p_fmt_in = fmt_in;
    es_format_Init( &p_dec->fmt_out, p_fmt->i_cat, 0 );
}

void decoder_Clean( decoder_t *p_dec )
{
    if ( p_dec->p_module != NULL )
    {
        module_unneed(p_dec, p_dec->p_module);
        p_dec->p_module = NULL;
    }

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
    vlc_assert( dec->p_fmt_in->i_cat == VIDEO_ES && dec->cbs != NULL );
    if ( unlikely(dec->p_fmt_in->i_cat != VIDEO_ES || dec->cbs == NULL) )
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
        if( dec->p_fmt_in->video.i_visible_width &&
            dec->p_fmt_in->video.i_visible_height )
        {
            dec->fmt_out.video.i_visible_width  = dec->p_fmt_in->video.i_visible_width;
            dec->fmt_out.video.i_visible_height = dec->p_fmt_in->video.i_visible_height;
            dec->fmt_out.video.i_x_offset       = dec->p_fmt_in->video.i_x_offset;
            dec->fmt_out.video.i_y_offset       = dec->p_fmt_in->video.i_y_offset;
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
    vlc_assert( dec->p_fmt_in->i_cat == VIDEO_ES && dec->cbs != NULL );
    if (dec->cbs->video.buffer_new == NULL)
        return picture_NewFromFormat( &dec->fmt_out.video );
    return dec->cbs->video.buffer_new( dec );
}

/** encoder **/
vlc_decoder_device *vlc_encoder_GetDecoderDevice( encoder_t *enc )
{
    vlc_assert( enc->fmt_in.i_cat == VIDEO_ES && enc->cbs != NULL );
    if ( unlikely(enc->fmt_in.i_cat != VIDEO_ES || enc->cbs == NULL ) )
        return NULL;

    return enc->cbs->video.get_device( enc );
}

void vlc_encoder_Destroy(encoder_t *encoder)
{
    if (encoder->ops != NULL && encoder->ops->close != NULL)
        encoder->ops->close(encoder);

    es_format_Clean(&encoder->fmt_in);
    es_format_Clean(&encoder->fmt_out);

    vlc_objres_clear(VLC_OBJECT(encoder));
    vlc_object_delete(encoder);
}
