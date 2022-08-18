/*****************************************************************************
 * video.c: transcoding stream output module (video)
 *****************************************************************************
 * Copyright (C) 2003-2009 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Jean-Paul Saman <jpsaman #_at_# m2x dot nl>
 *          Antoine Cellerier <dionoea at videolan dot org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_meta.h>
#include <vlc_spu.h>
#include <vlc_modules.h>
#include <vlc_sout.h>

#include "transcode.h"

#include <math.h>

struct encoder_owner
{
    encoder_t enc;
    sout_stream_id_sys_t *id;
};

static vlc_decoder_device *TranscodeHoldDecoderDevice(vlc_object_t *o, sout_stream_id_sys_t *id)
{
    if (id->dec_dev == NULL)
        id->dec_dev = vlc_decoder_device_Create( o, NULL );
    return id->dec_dev ? vlc_decoder_device_Hold(id->dec_dev) : NULL;
}

static inline struct encoder_owner *enc_get_owner( encoder_t *p_enc )
{
    return container_of( p_enc, struct encoder_owner, enc );
}

static vlc_decoder_device *video_get_encoder_device( encoder_t *enc )
{
    struct encoder_owner *p_owner = enc_get_owner( enc );
    if (p_owner->id->dec_dev == NULL)
        p_owner->id->dec_dev = vlc_decoder_device_Create( &enc->obj, NULL );

    return p_owner->id->dec_dev ? vlc_decoder_device_Hold(p_owner->id->dec_dev) : NULL;
}

static const struct encoder_owner_callbacks encoder_video_transcode_cbs = {
    { video_get_encoder_device, }
};

static vlc_decoder_device * video_get_decoder_device( decoder_t *p_dec )
{
    if( !var_InheritBool( p_dec, "hw-dec" ) )
        return NULL;

    struct decoder_owner *p_owner = dec_get_owner( p_dec );
    return TranscodeHoldDecoderDevice(&p_dec->obj, p_owner->id);
}

static void debug_format( vlc_object_t *p_obj, const es_format_t *fmt )
{
    msg_Dbg( p_obj, "format now %4.4s/%4.4s %dx%d(%dx%d) ø%d",
             (const char *) &fmt->i_codec,
             (const char *) &fmt->video.i_chroma,
             fmt->video.i_visible_width, fmt->video.i_visible_height,
             fmt->video.i_width, fmt->video.i_height,
             fmt->video.orientation );
}

static picture_t *transcode_video_filter_buffer_new( filter_t *p_filter )
{
    assert(p_filter->fmt_out.video.i_chroma == p_filter->fmt_out.i_codec);
    return picture_NewFromFormat( &p_filter->fmt_out.video );
}

static vlc_decoder_device * transcode_video_filter_hold_device(vlc_object_t *o, void *sys)
{
    sout_stream_id_sys_t *id = sys;
    return TranscodeHoldDecoderDevice(o, id);
}

static const struct filter_video_callbacks transcode_filter_video_cbs =
{
    transcode_video_filter_buffer_new, transcode_video_filter_hold_device,
};

static int transcode_video_filters_init( sout_stream_t *p_stream,
                                         const sout_filters_config_t *p_cfg,
                                         const es_format_t *p_src,
                                         vlc_video_context *src_ctx,
                                         const es_format_t *p_dst,
                                         sout_stream_id_sys_t *id );

static int video_update_format_decoder( decoder_t *p_dec, vlc_video_context *vctx )
{
    struct decoder_owner *p_owner = dec_get_owner( p_dec );
    sout_stream_id_sys_t *id = p_owner->id;

    vlc_mutex_lock(&id->fifo.lock);
    if( id->encoder != NULL && transcode_encoder_opened( id->encoder ) )
    {
        if( video_format_IsSimilar( &p_dec->fmt_out.video, &id->decoder_out.video ) )
        {
            vlc_mutex_unlock(&id->fifo.lock);
            goto end;
        }

        transcode_remove_filters( &id->p_final_conv_static );
        transcode_remove_filters( &id->p_uf_chain );
        transcode_remove_filters( &id->p_f_chain );
    }
    else if( id->encoder == NULL )
    {
        struct encoder_owner *p_enc_owner =
           (struct encoder_owner *)sout_EncoderCreate( VLC_OBJECT(p_owner->p_stream), sizeof(struct encoder_owner) );
        if ( unlikely(p_enc_owner == NULL))
            return VLC_EGENERIC;

        id->encoder = transcode_encoder_new( &p_enc_owner->enc, &p_dec->fmt_out );
        if( !id->encoder )
        {
            vlc_object_delete( &p_enc_owner->enc );
            return VLC_EGENERIC;
        }

        p_enc_owner->id = id;
        p_enc_owner->enc.cbs = &encoder_video_transcode_cbs;
    }


    es_format_Clean( &id->decoder_out );
    es_format_Copy( &id->decoder_out, &p_dec->fmt_out );
    /* crap, decoders resetting the whole fmtout... */
    es_format_SetMeta( &id->decoder_out, &p_dec->fmt_in );

    if( transcode_video_filters_init( p_owner->p_stream,
                  id->p_filterscfg,
                  &id->decoder_out,
                  vctx,
                  &id->decoder_out, id) != VLC_SUCCESS )
    {
        msg_Err(p_dec, "Could not update transcode chain to new format");
        goto error;
    }

    struct vlc_video_context *enc_vctx = NULL;
    const es_format_t *out_fmt;

    if( id->p_uf_chain )
    {
        enc_vctx = filter_chain_GetVideoCtxOut( id->p_uf_chain );
        out_fmt = filter_chain_GetFmtOut( id->p_uf_chain );
    }
    else if( id->p_f_chain )
    {
        enc_vctx = filter_chain_GetVideoCtxOut( id->p_f_chain );
        out_fmt = filter_chain_GetFmtOut( id->p_f_chain );
    }
    else
    {
        enc_vctx = vctx; /* Decoder video context */
        out_fmt = &id->decoder_out;
    }

    if( !transcode_encoder_opened( id->encoder ) )
    {
        transcode_encoder_video_configure( VLC_OBJECT(p_owner->p_stream),
                   &id->p_decoder->fmt_out.video,
                   id->p_enccfg,
                   &out_fmt->video,
                   enc_vctx,
                   id->encoder);

        if( transcode_encoder_open( id->encoder, id->p_enccfg ) != VLC_SUCCESS )
            goto error;
    }

    const es_format_t *encoder_fmt = transcode_encoder_format_in( id->encoder );

    if( !video_format_IsSimilar(&encoder_fmt->video, &out_fmt->video) )
    {
        filter_owner_t chain_owner = {
           .video = &transcode_filter_video_cbs,
           .sys = id,
        };

        if ( !id->p_final_conv_static )
            id->p_final_conv_static =
               filter_chain_NewVideo( p_owner->p_stream, false, &chain_owner );
         filter_chain_Reset( id->p_final_conv_static,
               out_fmt,
               enc_vctx,
               encoder_fmt);
         if( filter_chain_AppendConverter( id->p_final_conv_static, NULL ) != VLC_SUCCESS )
             goto error;
    }
    vlc_mutex_unlock(&id->fifo.lock);

    if( !id->downstream_id )
        id->downstream_id =
            id->pf_transcode_downstream_add( p_owner->p_stream,
                                             &id->p_decoder->fmt_in,
                                             transcode_encoder_format_out( id->encoder ) );
    msg_Info( p_dec, "video format update succeed" );

end:
    return VLC_SUCCESS;

error:
    transcode_remove_filters( &id->p_final_conv_static );

    if( transcode_encoder_opened( id->encoder ) )
        transcode_encoder_close( id->encoder );

    transcode_remove_filters( &id->p_uf_chain );
    transcode_remove_filters( &id->p_f_chain );

    vlc_mutex_unlock( &id->fifo.lock );

    return VLC_EGENERIC;
}

static picture_t *video_new_buffer_encoder( transcode_encoder_t *p_enc )
{
    return picture_NewFromFormat( &transcode_encoder_format_in( p_enc )->video );
}

static int transcode_process_picture( sout_stream_id_sys_t *id,
                                      picture_t *p_pic, block_t **out);

static void decoder_queue_video( decoder_t *p_dec, picture_t *p_pic )
{
    struct decoder_owner *p_owner = dec_get_owner( p_dec );
    sout_stream_id_sys_t *id = p_owner->id;

    block_t *p_block = NULL;
    int ret = transcode_process_picture( id, p_pic, &p_block );

    if( p_block == NULL )
        return;

    vlc_fifo_Lock( id->output_fifo );
    id->b_error |= ret != VLC_SUCCESS;
    if( id->b_error )
    {
        vlc_fifo_Unlock( id->output_fifo );
        block_ChainRelease( p_block );
        return;
    }

    vlc_fifo_QueueUnlocked( id->output_fifo, p_block );
    vlc_fifo_Unlock( id->output_fifo );
}

int transcode_video_init( sout_stream_t *p_stream, const es_format_t *p_fmt,
                          sout_stream_id_sys_t *id )
{
    msg_Dbg( p_stream,
             "creating video transcoding from fcc=`%4.4s' to fcc=`%4.4s'",
             (char*)&p_fmt->i_codec, (char*)&id->p_enccfg->i_codec );

    vlc_picture_chain_Init( &id->fifo.pic );
    id->output_fifo = block_FifoNew();
    if( id->output_fifo == NULL )
        return VLC_ENOMEM;

    id->b_transcode = true;
    es_format_Init( &id->decoder_out, VIDEO_ES, 0 );

    /* Open decoder
     */
    dec_get_owner( id->p_decoder )->id = id;

    static const struct decoder_owner_callbacks dec_cbs =
    {
        .video = {
            .get_device = video_get_decoder_device,
            .format_update = video_update_format_decoder,
            .queue = decoder_queue_video,
        },
    };
    id->p_decoder->cbs = &dec_cbs;

    id->p_decoder->pf_decode = NULL;
    id->p_decoder->pf_get_cc = NULL;

    id->p_decoder->p_module =
        module_need_var( id->p_decoder, "video decoder", "codec" );

    if( !id->p_decoder->p_module )
    {
        msg_Err( p_stream, "cannot find video decoder" );
        es_format_Clean( &id->decoder_out );
        return VLC_EGENERIC;
    }
    if( id->decoder_out.i_codec == 0 ) /* format_update can happen on open() */
    {
        es_format_Clean( &id->decoder_out );
        es_format_Copy( &id->decoder_out, &id->p_decoder->fmt_out );
    }

    return VLC_SUCCESS;
}

static int transcode_video_filters_init( sout_stream_t *p_stream,
                                         const sout_filters_config_t *p_cfg,
                                         const es_format_t *p_src,
                                         vlc_video_context *src_ctx,
                                         const es_format_t *p_dst,
                                         sout_stream_id_sys_t *id )
{
    /* Build chain */
    filter_owner_t owner = {
        .video = &transcode_filter_video_cbs,
        .sys = id,
    };
    id->p_f_chain = filter_chain_NewVideo( p_stream, false, &owner );
    if( !id->p_f_chain )
        return VLC_EGENERIC;
    filter_chain_Reset( id->p_f_chain, p_src, src_ctx, p_src );

    /* Deinterlace */
    if( p_cfg->video.psz_deinterlace != NULL )
    {
        filter_chain_AppendFilter( id->p_f_chain,
                                   p_cfg->video.psz_deinterlace,
                                   p_cfg->video.p_deinterlace_cfg,
                                   p_src );
        p_src = filter_chain_GetFmtOut( id->p_f_chain );
        src_ctx = filter_chain_GetVideoCtxOut( id->p_f_chain );
    }

    if( id->p_enccfg->video.fps.num > 0 &&
        id->p_enccfg->video.fps.den > 0 &&
      ( id->p_enccfg->video.fps.num != p_src->video.i_frame_rate ||
        id->p_enccfg->video.fps.den != p_src->video.i_frame_rate_base ) )
    {
        es_format_t dst;
        es_format_Copy(&dst, p_src);
        dst.video.i_frame_rate = id->p_enccfg->video.fps.num;
        dst.video.i_frame_rate_base = id->p_enccfg->video.fps.den;
        filter_chain_AppendFilter( id->p_f_chain, "fps", NULL, &dst );
        p_src = filter_chain_GetFmtOut( id->p_f_chain );
        src_ctx = filter_chain_GetVideoCtxOut( id->p_f_chain );
        es_format_Clean(&dst);
    }

    /* User filters */
    if( p_cfg->psz_filters )
    {
        msg_Dbg( p_stream, "adding user filters" );
        id->p_uf_chain = filter_chain_NewVideo( p_stream, true, &owner );
        if(!id->p_uf_chain)
            return VLC_EGENERIC;
        filter_chain_Reset( id->p_uf_chain, p_src, src_ctx, p_dst );
        filter_chain_AppendFromString( id->p_uf_chain, p_cfg->psz_filters );
        p_src = filter_chain_GetFmtOut( id->p_uf_chain );
        debug_format( VLC_OBJECT(p_stream), p_src );
   }

    /* Update encoder so it matches filters output */
    transcode_encoder_update_format_in( id->encoder, p_src, id->p_enccfg );

    /* SPU Sources */
    if( p_cfg->video.psz_spu_sources )
    {
        if( id->p_spu || (id->p_spu = spu_Create( p_stream, NULL )) )
            spu_ChangeSources( id->p_spu, p_cfg->video.psz_spu_sources );
    }

    return VLC_SUCCESS;
}

void transcode_video_clean( sout_stream_id_sys_t *id )
{
    /* Close encoder, but only if one was opened. */
    if ( id->encoder )
    {
        transcode_encoder_close( id->encoder );
        transcode_encoder_delete( id->encoder );
    }

    es_format_Clean( &id->decoder_out );

    /* Close filters */
    transcode_remove_filters( &id->p_f_chain );
    transcode_remove_filters( &id->p_uf_chain );
    transcode_remove_filters( &id->p_final_conv_static );
    if( id->p_spu_blender )
        filter_DeleteBlend( id->p_spu_blender );
    if( id->p_spu )
        spu_Destroy( id->p_spu );
    if ( id->dec_dev )
        vlc_decoder_device_Release( id->dec_dev );

    block_FifoRelease(id->output_fifo);
}

void transcode_video_push_spu( sout_stream_t *p_stream, sout_stream_id_sys_t *id,
                               subpicture_t *p_subpicture )
{
    if( !id->p_spu )
        id->p_spu = spu_Create( p_stream, NULL );
    if( !id->p_spu )
        subpicture_Delete( p_subpicture );
    else
        spu_PutSubpicture( id->p_spu, p_subpicture );
}

int transcode_video_get_output_dimensions( sout_stream_id_sys_t *id,
                                           unsigned *w, unsigned *h )
{
    vlc_mutex_lock( &id->fifo.lock );
    *w = id->decoder_out.video.i_visible_width;
    *h = id->decoder_out.video.i_visible_height;
    vlc_mutex_unlock( &id->fifo.lock );
    return (*w && *h) ? VLC_SUCCESS : VLC_EGENERIC;
}

static picture_t * RenderSubpictures( sout_stream_id_sys_t *id, picture_t *p_pic )
{
    if( !id->p_spu )
        return p_pic;

    /* Check if we have a subpicture to overlay */
    video_format_t fmt, outfmt;
    vlc_mutex_lock( &id->fifo.lock );
    video_format_Copy( &outfmt, &id->decoder_out.video );
    vlc_mutex_unlock( &id->fifo.lock );
    video_format_Copy( &fmt, &p_pic->format );
    if( fmt.i_visible_width <= 0 || fmt.i_visible_height <= 0 )
    {
        fmt.i_visible_width  = fmt.i_width;
        fmt.i_visible_height = fmt.i_height;
        fmt.i_x_offset       = 0;
        fmt.i_y_offset       = 0;
    }

    subpicture_t *p_subpic = spu_Render( id->p_spu, NULL, &fmt,
                                         &outfmt, vlc_tick_now(), p_pic->date,
                                         false, false );

    /* Overlay subpicture */
    if( p_subpic )
    {
        if( filter_chain_IsEmpty( id->p_f_chain ) )
        {
            /* We can't modify the picture, we need to duplicate it,
                 * in this point the picture is already p_encoder->fmt.in format*/
            picture_t *p_tmp = video_new_buffer_encoder( id->encoder );
            if( likely( p_tmp ) )
            {
                picture_Copy( p_tmp, p_pic );
                picture_Release( p_pic );
                p_pic = p_tmp;
            }
        }
        if( unlikely( !id->p_spu_blender ) )
            id->p_spu_blender = filter_NewBlend( VLC_OBJECT( id->p_spu ), &fmt );
        if( likely( id->p_spu_blender ) )
            picture_BlendSubpicture( p_pic, id->p_spu_blender, p_subpic );
        subpicture_Delete( p_subpic );
    }
    video_format_Clean( &fmt );
    video_format_Clean( &outfmt );

    return p_pic;
}

static void tag_last_block_with_flag( block_t **out, int i_flag )
{
    block_t *p_last = *out;
    if( p_last )
    {
        while( p_last->p_next )
            p_last = p_last->p_next;
        p_last->i_flags |= i_flag;
    }
}

static int transcode_process_picture( sout_stream_id_sys_t *id,
                                      picture_t *p_pic, block_t **out)
{
    /* Run the filter and output chains; first with the picture,
     * and then with NULL as many times as we need until they
     * stop outputting frames.
     */
    for ( picture_t *p_in = p_pic ;; p_in = NULL /* drain second time */ )
    {
        /* Run filter chain */
        if( id->p_f_chain )
            p_in = filter_chain_VideoFilter( id->p_f_chain, p_in );

        if( !p_in )
            break;

        for( ;; p_in = NULL /* drain second time */ )
        {
            /* Run user specified filter chain */
            filter_chain_t * secondary_chains[] = { id->p_uf_chain,
                                                    id->p_final_conv_static };
            for( size_t i=0; i<ARRAY_SIZE(secondary_chains); i++ )
            {
                if( !secondary_chains[i] )
                    continue;
                p_in = filter_chain_VideoFilter( secondary_chains[i], p_in );
            }

            if( !p_in )
                break;

            /* Blend subpictures */
            p_in = RenderSubpictures( id, p_in );

            if( p_in )
            {
                /* If a packetizer is used, multiple blocks might be returned, in w */
                block_t *p_encoded = transcode_encoder_encode( id->encoder, p_in );
                picture_Release( p_in );
                block_ChainAppend( out, p_encoded );
            }
        }
    }

    return VLC_SUCCESS;
}

int transcode_video_process( sout_stream_t *p_stream, sout_stream_id_sys_t *id,
                                    block_t *in, block_t **out )
{
    *out = NULL;

    bool b_eos = in && (in->i_flags & BLOCK_FLAG_END_OF_SEQUENCE);

    int ret = id->p_decoder->pf_decode( id->p_decoder, in );
    if( ret != VLCDEC_SUCCESS )
        return VLC_EGENERIC;

    /*
     * Encoder creation depends on decoder's update_format which is only
     * created once a few frames have been passed to the decoder.
     */
    if( id->encoder == NULL )
        return VLC_SUCCESS;

    vlc_fifo_Lock( id->output_fifo );
    if( unlikely( !id->b_error && in == NULL ) && transcode_encoder_opened( id->encoder ) )
    {
        msg_Dbg( p_stream, "Flushing thread and waiting that");
        if( transcode_encoder_drain( id->encoder, out ) == VLC_SUCCESS )
            msg_Dbg( p_stream, "Flushing done");
        else
            msg_Warn( p_stream, "Flushing failed");
    }
    bool has_error = id->b_error;
    if( !has_error )
    {
        vlc_frame_t *pendings = vlc_fifo_DequeueAllUnlocked( id->output_fifo );
        block_ChainAppend(out, pendings);
    }
    vlc_fifo_Unlock( id->output_fifo );

    if( b_eos )
        tag_last_block_with_flag( out, BLOCK_FLAG_END_OF_SEQUENCE );

    return has_error ? VLC_EGENERIC : VLC_SUCCESS;
}
