/*****************************************************************************
 * video.c: transcoding video encoder
 *****************************************************************************
 * Copyright (C) 2003-2009 VLC authors and VideoLAN
 *               2018 VideoLabs, VideoLAN and VLC authors
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Jean-Paul Saman <jpsaman #_at_# m2x dot nl>
 *          Antoine Cellerier <dionoea at videolan dot org>
 *          Ilkka Ollakka <ileoo at videolan dot org>
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
 * along with this program; if not, If not, see https://www.gnu.org/licenses/
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_modules.h>
#include <vlc_codec.h>
#include <vlc_sout.h>
#include "encoder.h"
#include "encoder_priv.h"

void transcode_video_framerate_apply( const video_format_t *p_src,
                                            video_format_t *p_dst )
{
    /* Handle frame rate conversion */
    if( !p_dst->i_frame_rate || !p_dst->i_frame_rate_base )
    {
        p_dst->i_frame_rate = p_src->i_frame_rate;
        p_dst->i_frame_rate_base = p_src->i_frame_rate_base;

        if( !p_dst->i_frame_rate || !p_dst->i_frame_rate_base )
        {
            /* Pick a sensible default value */
            p_dst->i_frame_rate = ENC_FRAMERATE;
            p_dst->i_frame_rate_base = ENC_FRAMERATE_BASE;
        }
    }

    vlc_ureduce( &p_dst->i_frame_rate, &p_dst->i_frame_rate_base,
                  p_dst->i_frame_rate,  p_dst->i_frame_rate_base, 0 );
}

static void transcode_video_scale_apply( vlc_object_t *p_obj,
                                        const video_format_t *p_src,
                                        float f_scale,
                                        unsigned i_maxwidth,
                                        unsigned i_maxheight,
                                        video_format_t *p_dst )
{
    /* Calculate scaling
     * width/height of source */
    unsigned i_src_width = p_src->i_visible_width ? p_src->i_visible_width : p_src->i_width;
    unsigned i_src_height = p_src->i_visible_height ? p_src->i_visible_height : p_src->i_height;

    /* with/height scaling */
    float f_scale_width = 1;
    float f_scale_height = 1;

    /* aspect ratio */
    float f_aspect = (double)p_src->i_sar_num * p_src->i_width /
                             p_src->i_sar_den / p_src->i_height;

    msg_Dbg( p_obj, "decoder aspect is %f:1", f_aspect );

    /* Change f_aspect from source frame to source pixel */
    f_aspect = f_aspect * i_src_height / i_src_width;
    msg_Dbg( p_obj, "source pixel aspect is %f:1", f_aspect );

    /* Calculate scaling factor for specified parameters */
    if( p_dst->i_visible_width == 0 && p_dst->i_visible_height == 0 && f_scale )
    {
        /* Global scaling. Make sure width will remain a factor of 16 */
        float f_real_scale;
        unsigned i_new_height;
        unsigned i_new_width = i_src_width * f_scale;

        if( i_new_width % 16 <= 7 && i_new_width >= 16 )
            i_new_width -= i_new_width % 16;
        else
            i_new_width += 16 - i_new_width % 16;

        f_real_scale = (float)( i_new_width ) / (float) i_src_width;

        i_new_height = __MAX( 16, i_src_height * (float)f_real_scale );

        f_scale_width = f_real_scale;
        f_scale_height = (float) i_new_height / (float) i_src_height;
    }
    else if( p_dst->i_visible_width && p_dst->i_visible_height == 0 )
    {
        /* Only width specified */
        f_scale_width = (float)p_dst->i_visible_width / i_src_width;
        f_scale_height = f_scale_width;
    }
    else if( p_dst->i_visible_width == 0 && p_dst->i_visible_height )
    {
         /* Only height specified */
         f_scale_height = (float)p_dst->i_visible_height / i_src_height;
         f_scale_width = f_scale_height;
     }
     else if( p_dst->i_visible_width && p_dst->i_visible_height )
     {
         /* Width and height specified */
         f_scale_width = (float)p_dst->i_visible_width / i_src_width;
         f_scale_height = (float)p_dst->i_visible_height / i_src_height;
     }

     /* check maxwidth and maxheight */
     if( i_maxwidth && f_scale_width > (float)i_maxwidth / i_src_width )
     {
         f_scale_width = (float)i_maxwidth / i_src_width;
     }

     if( i_maxheight && f_scale_height > (float)i_maxheight / i_src_height )
     {
         f_scale_height = (float)i_maxheight / i_src_height;
     }


     /* Change aspect ratio from source pixel to scaled pixel */
     f_aspect = f_aspect * f_scale_height / f_scale_width;
     msg_Dbg( p_obj, "scaled pixel aspect is %f:1", f_aspect );

     /* f_scale_width and f_scale_height are now final */
     /* Calculate width, height from scaling
      * Make sure its multiple of 2
      */
     /* width/height of output stream */
     unsigned i_dst_visible_width =  lroundf(f_scale_width*i_src_width);
     unsigned i_dst_visible_height = lroundf(f_scale_height*i_src_height);
     unsigned i_dst_width =  lroundf(f_scale_width*p_src->i_width);
     unsigned i_dst_height = lroundf(f_scale_height*p_src->i_height);

     if( i_dst_visible_width & 1 ) ++i_dst_visible_width;
     if( i_dst_visible_height & 1 ) ++i_dst_visible_height;
     if( i_dst_width & 1 ) ++i_dst_width;
     if( i_dst_height & 1 ) ++i_dst_height;

     /* Store calculated values */
     p_dst->i_width = i_dst_width;
     p_dst->i_visible_width = i_dst_visible_width;
     p_dst->i_height = i_dst_height;
     p_dst->i_visible_height = i_dst_visible_height;

     msg_Dbg( p_obj, "source %ux%u, destination %ux%u",
                     i_src_width, i_src_height,
                     i_dst_visible_width, i_dst_visible_height );
}

void transcode_video_sar_apply( const video_format_t *p_src,
                                      video_format_t *p_dst )
{
    /* Check whether a particular aspect ratio was requested */
    if( p_dst->i_sar_num <= 0 || p_dst->i_sar_den <= 0 )
    {
        vlc_ureduce( &p_dst->i_sar_num, &p_dst->i_sar_den,
                     (uint64_t)p_src->i_sar_num * (p_dst->i_x_offset + p_dst->i_visible_width)
                                                * (p_src->i_x_offset + p_src->i_visible_height),
                     (uint64_t)p_src->i_sar_den * (p_dst->i_y_offset + p_dst->i_visible_height)
                                                * (p_src->i_y_offset + p_src->i_visible_width),
                     0 );
    }
    else
    {
        vlc_ureduce( &p_dst->i_sar_num, &p_dst->i_sar_den,
                     p_dst->i_sar_num, p_dst->i_sar_den, 0 );
    }
}

static void transcode_video_size_config_apply( vlc_object_t *p_obj,
                                               const video_format_t *p_srcref,
                                               const transcode_encoder_config_t *p_cfg,
                                               video_format_t *p_dst )
{
    if( !p_cfg->video.f_scale &&
        (p_cfg->video.i_width & ~1) && (p_cfg->video.i_width & ~1) )
    {
        p_dst->i_width = p_dst->i_visible_width = p_cfg->video.i_width & ~1;
        p_dst->i_height = p_dst->i_visible_height = p_cfg->video.i_height & ~1;
    }
    else if( p_cfg->video.f_scale )
    {
        transcode_video_scale_apply( p_obj,
                                     p_srcref,
                                     p_cfg->video.f_scale,
                                     p_cfg->video.i_maxwidth,
                                     p_cfg->video.i_maxheight,
                                     p_dst );
    }
    else
    {
        p_dst->i_width = p_srcref->i_width;
        p_dst->i_visible_width = p_srcref->i_visible_width;
        p_dst->i_height = p_srcref->i_height;
        p_dst->i_visible_height = p_srcref->i_visible_height;
    }
}

void transcode_encoder_video_configure( vlc_object_t *p_obj,
                                        const video_format_t *p_dec_out,
                                        const transcode_encoder_config_t *p_cfg,
                                        const video_format_t *p_src,
                                        vlc_video_context *vctx_in,
                                        transcode_encoder_t *p_enc )
{
    video_format_t *p_enc_in = &p_enc->p_encoder->fmt_in.video;
    video_format_t *p_enc_out = &p_enc->p_encoder->fmt_out.video;

    /* Complete destination format */
    p_enc->p_encoder->fmt_out.i_codec = p_enc_out->i_chroma = p_cfg->i_codec;
    p_enc->p_encoder->fmt_out.i_bitrate = p_cfg->video.i_bitrate;
    p_enc_out->i_sar_num = p_enc_out->i_sar_den = 0;
    if( p_cfg->video.fps.num )
    {
        p_enc_in->i_frame_rate = p_enc_out->i_frame_rate =
                p_cfg->video.fps.num;
        p_enc_in->i_frame_rate_base = p_enc_out->i_frame_rate_base =
                __MAX(p_cfg->video.fps.den, 1);
    }

    /* Complete source format */
    p_enc_in->orientation = ORIENT_NORMAL;
    p_enc_out->orientation = p_enc_in->orientation;

    p_enc_in->i_chroma = p_enc->p_encoder->fmt_in.i_codec;

    transcode_video_framerate_apply( p_src, p_enc_out );
    p_enc_in->i_frame_rate = p_enc_out->i_frame_rate;
    p_enc_in->i_frame_rate_base = p_enc_out->i_frame_rate_base;
    msg_Dbg( p_obj, "source fps %u/%u, destination %u/%u",
             p_dec_out->i_frame_rate, p_dec_out->i_frame_rate_base,
             p_enc_in->i_frame_rate, p_enc_in->i_frame_rate_base );

    /* Modify to requested sizes/scale */
    transcode_video_size_config_apply( p_obj, p_src, p_cfg, p_enc_out );
    /* Propagate sizing to output */
    p_enc_in->i_width = p_enc_out->i_width;
    p_enc_in->i_visible_width = p_enc_out->i_visible_width;
    p_enc_in->i_height = p_enc_out->i_height;
    p_enc_in->i_visible_height = p_enc_out->i_visible_height;

    transcode_video_sar_apply( p_src, p_enc_out );
    p_enc_in->i_sar_num = p_enc_out->i_sar_num;
    p_enc_in->i_sar_den = p_enc_out->i_sar_den;
    msg_Dbg( p_obj, "encoder aspect is %u:%u",
             p_enc_out->i_sar_num * p_enc_out->i_width,
             p_enc_out->i_sar_den * p_enc_out->i_height );

    p_enc->p_encoder->vctx_in = vctx_in;

    /* Keep colorspace etc info along */
    p_enc_out->space     = p_src->space;
    p_enc_out->transfer  = p_src->transfer;
    p_enc_out->primaries = p_src->primaries;
    p_enc_out->color_range = p_src->color_range;

     /* set masks when RGB */
    video_format_FixRgb(&p_enc->p_encoder->fmt_in.video);
    video_format_FixRgb(&p_enc->p_encoder->fmt_out.video);

    if ( p_cfg->psz_lang )
    {
        free( p_enc->p_encoder->fmt_in.psz_language );
        free( p_enc->p_encoder->fmt_out.psz_language );
        p_enc->p_encoder->fmt_in.psz_language = strdup( p_cfg->psz_lang );
        p_enc->p_encoder->fmt_out.psz_language = strdup( p_cfg->psz_lang );
    }

    msg_Dbg( p_obj, "source chroma: %4.4s, destination %4.4s",
             (const char *)&p_dec_out->i_chroma,
             (const char *)&p_enc_in->i_chroma);
}

int transcode_encoder_video_test( encoder_t *p_encoder,
                                  const transcode_encoder_config_t *p_cfg,
                                  const es_format_t *p_dec_fmtin,
                                  vlc_fourcc_t i_codec_in,
                                  es_format_t *p_enc_wanted_in )
{
    p_encoder->i_threads = p_cfg->video.threads.i_count;
    p_encoder->p_cfg = p_cfg->p_config_chain;

    es_format_Init( &p_encoder->fmt_in, VIDEO_ES, i_codec_in );
    es_format_Init( &p_encoder->fmt_out, VIDEO_ES, p_cfg->i_codec );

    const video_format_t *p_dec_in = &p_dec_fmtin->video;
    video_format_t *p_vfmt_in = &p_encoder->fmt_in.video;
    video_format_t *p_vfmt_out = &p_encoder->fmt_out.video;

    /* Requested output */
    p_vfmt_out->i_width = p_cfg->video.i_width & ~1;
    p_vfmt_out->i_height = p_cfg->video.i_height & ~1;
    p_encoder->fmt_out.i_bitrate = p_cfg->video.i_bitrate;

    /* The dimensions will be set properly later on.
     * Just put sensible values so we can test an encoder is available. */
    /* Input */
    p_vfmt_in->i_chroma = i_codec_in;
    p_vfmt_in->i_width = FIRSTVALID( p_vfmt_out->i_width, p_dec_in->i_width, 16 ) & ~1;
    p_vfmt_in->i_height = FIRSTVALID( p_vfmt_out->i_height, p_dec_in->i_height, 16 ) & ~1;
    p_vfmt_in->i_visible_width = FIRSTVALID( p_vfmt_out->i_visible_width,
                                             p_dec_in->i_visible_width, p_vfmt_in->i_width ) & ~1;
    p_vfmt_in->i_visible_height = FIRSTVALID( p_vfmt_out->i_visible_height,
                                              p_dec_in->i_visible_height, p_vfmt_in->i_height ) & ~1;
    p_vfmt_in->i_frame_rate = ENC_FRAMERATE;
    p_vfmt_in->i_frame_rate_base = ENC_FRAMERATE_BASE;

    module_t *p_module = module_need( p_encoder, "encoder", p_cfg->psz_name, true );
    if( !p_module )
    {
        msg_Err( p_encoder, "cannot find video encoder (module:%s fourcc:%4.4s). "
                           "Take a look few lines earlier to see possible reason.",
                 p_cfg->psz_name ? p_cfg->psz_name : "any",
                 (char *)&p_cfg->i_codec );
    }
    else
    {
        /* Close the encoder.
         * We'll open it only when we have the first frame. */
        module_unneed( p_encoder, p_module );
    }

    if( likely(!p_encoder->fmt_in.video.i_chroma) ) /* always missing, and required by filter chain */
        p_encoder->fmt_in.video.i_chroma = p_encoder->fmt_in.i_codec;

    /* output our requested format */
    es_format_Copy( p_enc_wanted_in, &p_encoder->fmt_in );
    video_format_FixRgb( &p_enc_wanted_in->video ); /* set masks when RGB */

    es_format_Clean( &p_encoder->fmt_in );
    es_format_Clean( &p_encoder->fmt_out );

    vlc_object_delete(p_encoder);

    return p_module != NULL ? VLC_SUCCESS : VLC_EGENERIC;
}

static void* EncoderThread( void *obj )
{
    transcode_encoder_t *p_enc = obj;
    picture_t *p_pic = NULL;
    int canc = vlc_savecancel ();
    block_t *p_block = NULL;

    vlc_mutex_lock( &p_enc->lock_out );

    for( ;; )
    {
        while( !p_enc->b_abort &&
               (p_pic = picture_fifo_Pop( p_enc->pp_pics )) == NULL )
            vlc_cond_wait( &p_enc->cond, &p_enc->lock_out );
        vlc_sem_post( &p_enc->picture_pool_has_room );

        if( p_pic )
        {
            /* release lock while encoding */
            vlc_mutex_unlock( &p_enc->lock_out );
            p_block = p_enc->p_encoder->pf_encode_video( p_enc->p_encoder, p_pic );
            picture_Release( p_pic );
            vlc_mutex_lock( &p_enc->lock_out );

            block_ChainAppend( &p_enc->p_buffers, p_block );
        }

        if( p_enc->b_abort )
            break;
    }

    /*Encode what we have in the buffer on closing*/
    while( (p_pic = picture_fifo_Pop( p_enc->pp_pics )) != NULL )
    {
        vlc_sem_post( &p_enc->picture_pool_has_room );
        p_block = p_enc->p_encoder->pf_encode_video( p_enc->p_encoder, p_pic );
        picture_Release( p_pic );
        block_ChainAppend( &p_enc->p_buffers, p_block );
    }

    /*Now flush encoder*/
    do {
        p_block = p_enc->p_encoder->pf_encode_video(p_enc->p_encoder, NULL );
        block_ChainAppend( &p_enc->p_buffers, p_block );
    } while( p_block );

    vlc_mutex_unlock( &p_enc->lock_out );

    vlc_restorecancel (canc);

    return NULL;
}

int transcode_encoder_video_drain( transcode_encoder_t *p_enc, block_t **out )
{
    if( !p_enc->b_threaded )
    {
        block_t *p_block;
        do {
            p_block = transcode_encoder_encode( p_enc, NULL );
            block_ChainAppend( out, p_block );
        } while( p_block );
    }
    else
    {
        if( p_enc->b_threaded && !p_enc->b_abort )
        {
            vlc_mutex_lock( &p_enc->lock_out );
            p_enc->b_abort = true;
            vlc_cond_signal( &p_enc->cond );
            vlc_mutex_unlock( &p_enc->lock_out );
            vlc_join( p_enc->thread, NULL );
        }
        block_ChainAppend( out, transcode_encoder_get_output_async( p_enc ) );
    }
    return VLC_SUCCESS;
}

void transcode_encoder_video_close( transcode_encoder_t *p_enc )
{
    if( p_enc->b_threaded && !p_enc->b_abort )
    {
        vlc_mutex_lock( &p_enc->lock_out );
        p_enc->b_abort = true;
        vlc_cond_signal( &p_enc->cond );
        vlc_mutex_unlock( &p_enc->lock_out );
        vlc_join( p_enc->thread, NULL );
    }

    /* Close encoder */
    module_unneed( p_enc->p_encoder, p_enc->p_encoder->p_module );
    p_enc->p_encoder->p_module = NULL;
}

int transcode_encoder_video_open( transcode_encoder_t *p_enc,
                                   const transcode_encoder_config_t *p_cfg )
{
    p_enc->p_encoder->i_threads = p_cfg->video.threads.i_count;
    p_enc->p_encoder->p_cfg = p_cfg->p_config_chain;

    p_enc->p_encoder->p_module =
        module_need( p_enc->p_encoder, "encoder", p_cfg->psz_name, true );
    if( !p_enc->p_encoder->p_module )
        return VLC_EGENERIC;

    p_enc->p_encoder->fmt_in.video.i_chroma = p_enc->p_encoder->fmt_in.i_codec;

    /*  */
    p_enc->p_encoder->fmt_out.i_codec =
        vlc_fourcc_GetCodec( VIDEO_ES, p_enc->p_encoder->fmt_out.i_codec );

    vlc_sem_init( &p_enc->picture_pool_has_room, p_cfg->video.threads.pool_size );
    vlc_cond_init( &p_enc->cond );
    p_enc->p_buffers = NULL;
    p_enc->b_abort = false;

    if( p_cfg->video.threads.i_count > 0 )
    {
        if( vlc_clone( &p_enc->thread, EncoderThread, p_enc, p_cfg->video.threads.i_priority ) )
        {
            module_unneed( p_enc->p_encoder, p_enc->p_encoder->p_module );
            p_enc->p_encoder->p_module = NULL;
            return VLC_EGENERIC;
        }
        p_enc->b_threaded = true;
    }

    return VLC_SUCCESS;
}

block_t * transcode_encoder_video_encode( transcode_encoder_t *p_enc, picture_t *p_pic )
{
    if( !p_enc->b_threaded )
    {
        return p_enc->p_encoder->pf_encode_video( p_enc->p_encoder, p_pic );
    }

    vlc_sem_wait( &p_enc->picture_pool_has_room );
    vlc_mutex_lock( &p_enc->lock_out );
    picture_Hold( p_pic );
    picture_fifo_Push( p_enc->pp_pics, p_pic );
    vlc_cond_signal( &p_enc->cond );
    vlc_mutex_unlock( &p_enc->lock_out );
    return NULL;
}
