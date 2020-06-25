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
    struct decoder_owner *p_owner = dec_get_owner( p_dec );
    return TranscodeHoldDecoderDevice(&p_dec->obj, p_owner->id);
}

static void debug_format( sout_stream_t *p_stream, const es_format_t *fmt )
{
    msg_Dbg( p_stream, "format now %4.4s/%4.4s %dx%d(%dx%d) ø%d",
             (const char *) &fmt->i_codec,
             (const char *) &fmt->video.i_chroma,
             fmt->video.i_visible_width, fmt->video.i_visible_height,
             fmt->video.i_width, fmt->video.i_height,
             fmt->video.orientation );
}

static vlc_decoder_device * transcode_video_filter_hold_device(vlc_object_t *o, void *sys)
{
    sout_stream_id_sys_t *id = sys;
    return TranscodeHoldDecoderDevice(o, id);
}

static int video_update_format_decoder( decoder_t *p_dec, vlc_video_context *vctx )
{
    struct decoder_owner *p_owner = dec_get_owner( p_dec );
    sout_stream_id_sys_t *id = p_owner->id;
    vlc_object_t        *p_obj = p_owner->p_obj;
    filter_chain_t       *test_chain;

    vlc_mutex_lock( &id->fifo.lock );

    const es_format_t *p_enc_in = transcode_encoder_format_in( id->encoder );

    if( p_enc_in->i_codec == p_dec->fmt_out.i_codec ||
        video_format_IsSimilar( &id->decoder_out.video, &p_dec->fmt_out.video ) )
    {
        vlc_mutex_unlock( &id->fifo.lock );
        return 0;
    }

    id->decoder_vctx_out = vctx;
    es_format_Clean( &id->decoder_out );
    es_format_Copy( &id->decoder_out, &p_dec->fmt_out );

    /* crap, decoders resetting the whole fmtout... */
    es_format_SetMeta( &id->decoder_out, &p_dec->fmt_in );

    vlc_mutex_unlock( &id->fifo.lock );

    msg_Dbg( p_obj, "Checking if filter chain %4.4s -> %4.4s is possible",
                 (char *)&p_dec->fmt_out.i_codec, (char*)&p_enc_in->i_codec );
    test_chain = filter_chain_NewVideo( p_obj, false, NULL );
    filter_chain_Reset( test_chain, &p_dec->fmt_out, vctx, p_enc_in );

    int chain_works = filter_chain_AppendConverter( test_chain, p_enc_in );
    filter_chain_Delete( test_chain );

    msg_Dbg( p_obj, "Filter chain testing done, input chroma %4.4s seems to be %s for transcode",
                     (char *)&p_dec->fmt_out.video.i_chroma,
                     chain_works == 0 ? "possible" : "not possible");
    return chain_works;
}

static picture_t *video_new_buffer_encoder( transcode_encoder_t *p_enc )
{
    return picture_NewFromFormat( &transcode_encoder_format_in( p_enc )->video );
}

static picture_t *transcode_video_filter_buffer_new( filter_t *p_filter )
{
    assert(p_filter->fmt_out.video.i_chroma == p_filter->fmt_out.i_codec);
    return picture_NewFromFormat( &p_filter->fmt_out.video );
}

static void decoder_queue_video( decoder_t *p_dec, picture_t *p_pic )
{
    struct decoder_owner *p_owner = dec_get_owner( p_dec );
    sout_stream_id_sys_t *id = p_owner->id;

    vlc_mutex_lock(&id->fifo.lock);
    *id->fifo.pic.last = p_pic;
    id->fifo.pic.last = &p_pic->p_next;
    vlc_mutex_unlock(&id->fifo.lock);
}

static picture_t *transcode_dequeue_all_pics( sout_stream_id_sys_t *id )
{
    vlc_mutex_lock(&id->fifo.lock);
    picture_t *p_pics = id->fifo.pic.first;
    id->fifo.pic.first = NULL;
    id->fifo.pic.last = &id->fifo.pic.first;
    vlc_mutex_unlock(&id->fifo.lock);

    return p_pics;
}

int transcode_video_init( sout_stream_t *p_stream, const es_format_t *p_fmt,
                          sout_stream_id_sys_t *id )
{
    msg_Dbg( p_stream,
             "creating video transcoding from fcc=`%4.4s' to fcc=`%4.4s'",
             (char*)&p_fmt->i_codec, (char*)&id->p_enccfg->i_codec );

    id->fifo.pic.first = NULL;
    id->fifo.pic.last = &id->fifo.pic.first;
    id->b_transcode = true;
    es_format_Init( &id->decoder_out, VIDEO_ES, 0 );
    id->decoder_vctx_out = NULL;

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
        id->decoder_vctx_out = NULL /* TODO id->p_decoder->vctx_out*/;
    }

    /*
     * Open encoder.
     * Because some info about the decoded input will only be available
     * once the first frame is decoded, we actually only test the availability
     * of the encoder here.
     */

    /* Should be the same format until encoder loads */
    es_format_t encoder_tested_fmt_in;
    es_format_Init( &encoder_tested_fmt_in, id->decoder_out.i_cat, 0 );

    struct encoder_owner *p_enc_owner = (struct encoder_owner*)sout_EncoderCreate(p_stream, sizeof(struct encoder_owner));
    if ( unlikely(p_enc_owner == NULL))
       goto error;

    p_enc_owner->id = id;
    p_enc_owner->enc.cbs = &encoder_video_transcode_cbs;

    if( transcode_encoder_test( &p_enc_owner->enc,
                                id->p_enccfg,
                                &id->p_decoder->fmt_in,
                                id->p_decoder->fmt_out.i_codec,
                                &encoder_tested_fmt_in ) )
       goto error;

    p_enc_owner = (struct encoder_owner *)sout_EncoderCreate(p_stream, sizeof(struct encoder_owner));
    if ( unlikely(p_enc_owner == NULL))
       goto error;

    id->encoder = transcode_encoder_new( &p_enc_owner->enc, &encoder_tested_fmt_in );
    if( !id->encoder )
       goto error;

    p_enc_owner->id = id;
    p_enc_owner->enc.cbs = &encoder_video_transcode_cbs;

    /* Will use this format as encoder input for now */
    transcode_encoder_update_format_in( id->encoder, &encoder_tested_fmt_in );

    es_format_Clean( &encoder_tested_fmt_in );

    return VLC_SUCCESS;

error:
    module_unneed( id->p_decoder, id->p_decoder->p_module );
    id->p_decoder->p_module = NULL;
    es_format_Clean( &encoder_tested_fmt_in );
    es_format_Clean( &id->decoder_out );
    return VLC_EGENERIC;
}

static const struct filter_video_callbacks transcode_filter_video_cbs =
{
    transcode_video_filter_buffer_new, transcode_video_filter_hold_device,
};

/* Take care of the scaling and chroma conversions. */
static int transcode_video_set_conversions( sout_stream_t *p_stream,
                                            sout_stream_id_sys_t *id,
                                            const es_format_t **pp_src,
                                            vlc_video_context **pp_src_vctx,
                                            const es_format_t *p_dst,
                                            bool b_reorient )
{
    filter_owner_t owner = {
        .video = &transcode_filter_video_cbs,
        .sys = id,
    };

    enum
    {
        STEP_NONSTATIC = 0,
        STEP_STATIC,
    };
    for( int step = STEP_NONSTATIC; step <= STEP_STATIC; step++ )
    {
        const bool b_do_scale = (*pp_src)->video.i_width != p_dst->video.i_width ||
                                (*pp_src)->video.i_height != p_dst->video.i_height;
        const bool b_do_chroma = (*pp_src)->video.i_chroma != p_dst->video.i_chroma;
        const bool b_do_orient = ((*pp_src)->video.orientation != ORIENT_NORMAL) && b_reorient;

        if( step == STEP_STATIC && b_do_orient )
            return VLC_EGENERIC;

        const es_format_t *p_tmpdst = p_dst;

        if( ! (b_do_scale || b_do_chroma || b_do_orient) )
            return VLC_SUCCESS;

        es_format_t tmpdst;
        if( b_do_orient )
        {
            es_format_Init( &tmpdst, VIDEO_ES, p_dst->video.i_chroma );
            video_format_ApplyRotation( &tmpdst.video, &p_dst->video );
            p_tmpdst = &tmpdst;
        }

        msg_Dbg( p_stream, "adding (scale %d,chroma %d, orient %d) converters",
                 b_do_scale, b_do_chroma, b_do_orient );

        filter_chain_t **pp_chain = (step == STEP_NONSTATIC)
                ? &id->p_conv_nonstatic
                : &id->p_conv_static;

        *pp_chain = filter_chain_NewVideo( p_stream, step == STEP_NONSTATIC, &owner );
        if( !*pp_chain )
            return VLC_EGENERIC;
        filter_chain_Reset( *pp_chain, *pp_src, *pp_src_vctx, p_tmpdst );

        if( filter_chain_AppendConverter( *pp_chain, p_tmpdst ) != VLC_SUCCESS )
            return VLC_EGENERIC;

        *pp_src = filter_chain_GetFmtOut( *pp_chain );
        *pp_src_vctx = filter_chain_GetVideoCtxOut( *pp_chain );
        debug_format( p_stream, *pp_src );
    }

    return VLC_SUCCESS;
}

static inline bool transcode_video_filters_configured( const sout_stream_id_sys_t *id )
{
    return !!id->p_f_chain;
}

static int transcode_video_filters_init( sout_stream_t *p_stream,
                                         const sout_filters_config_t *p_cfg,
                                         bool b_master_sync,
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

    if( b_master_sync )
    {
        filter_chain_AppendFilter( id->p_f_chain, "fps", NULL, p_src );
        p_src = filter_chain_GetFmtOut( id->p_f_chain );
        src_ctx = filter_chain_GetVideoCtxOut( id->p_f_chain );
    }

    /* Chroma and other conversions */
    if( transcode_video_set_conversions( p_stream, id, &p_src, &src_ctx, p_dst,
                                         p_cfg->video.b_reorient ) != VLC_SUCCESS )
        return VLC_EGENERIC;

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
        debug_format( p_stream, p_src );
   }

    /* Update encoder so it matches filters output */
    transcode_encoder_update_format_in( id->encoder, p_src );

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
    /* Close encoder */
    transcode_encoder_close( id->encoder );
    transcode_encoder_delete( id->encoder );

    es_format_Clean( &id->decoder_out );

    /* Close filters */
    transcode_remove_filters( &id->p_f_chain );
    transcode_remove_filters( &id->p_conv_nonstatic );
    transcode_remove_filters( &id->p_conv_static );
    transcode_remove_filters( &id->p_uf_chain );
    transcode_remove_filters( &id->p_final_conv_static );
    if( id->p_spu_blender )
        filter_DeleteBlend( id->p_spu_blender );
    if( id->p_spu )
        spu_Destroy( id->p_spu );
    if ( id->dec_dev )
        vlc_decoder_device_Release( id->dec_dev );
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

int transcode_video_process( sout_stream_t *p_stream, sout_stream_id_sys_t *id,
                                    block_t *in, block_t **out )
{
    *out = NULL;

    bool b_eos = in && (in->i_flags & BLOCK_FLAG_END_OF_SEQUENCE);

    int ret = id->p_decoder->pf_decode( id->p_decoder, in );
    if( ret != VLCDEC_SUCCESS )
        return VLC_EGENERIC;

    picture_t *p_pics = transcode_dequeue_all_pics( id );

    do
    {
        picture_t *p_pic = p_pics;
        if( p_pic )
        {
            p_pics = p_pic->p_next;
            p_pic->p_next = NULL;
        }

        if( id->b_error && p_pic )
        {
            picture_Release( p_pic );
            continue;
        }

        if( p_pic && ( unlikely(!transcode_encoder_opened(id->encoder)) ||
              !video_format_IsSimilar( &id->decoder_out.video, &p_pic->format ) ) )
        {
            if( !transcode_encoder_opened(id->encoder) ) /* Configure Encoder input/output */
            {
                assert( !id->p_f_chain && !id->p_uf_chain );
                transcode_encoder_video_configure( VLC_OBJECT(p_stream),
                                                   &id->p_decoder->fmt_out.video,
                                                   id->p_enccfg,
                                                   &p_pic->format,
                                                   picture_GetVideoContext(p_pic),
                                                   id->encoder );
                /* will be opened below */
            }
            else /* picture format has changed */
            {
                msg_Info( p_stream, "aspect-ratio changed, reiniting. %i -> %i : %i -> %i.",
                            id->decoder_out.video.i_sar_num, p_pic->format.i_sar_num,
                            id->decoder_out.video.i_sar_den, p_pic->format.i_sar_den
                        );
                /* Close filters, encoder format input can't change */
                transcode_remove_filters( &id->p_f_chain );
                transcode_remove_filters( &id->p_conv_nonstatic );
                transcode_remove_filters( &id->p_conv_static );
                transcode_remove_filters( &id->p_uf_chain );
                transcode_remove_filters( &id->p_final_conv_static );
                if( id->p_spu_blender )
                    filter_DeleteBlend( id->p_spu_blender );
                id->p_spu_blender = NULL;

                video_format_Clean( &id->decoder_out.video );
            }

            video_format_Copy( &id->decoder_out.video, &p_pic->format );
            transcode_video_framerate_apply( &p_pic->format, &id->decoder_out.video );
            transcode_video_sar_apply( &p_pic->format, &id->decoder_out.video );
            id->decoder_vctx_out = picture_GetVideoContext(p_pic);

            if( !transcode_video_filters_configured( id ) )
            {
                if( transcode_video_filters_init( p_stream,
                                                  id->p_filterscfg,
                                                 (id->p_enccfg->video.fps.num > 0),
                                                 &id->decoder_out,
                                                 id->decoder_vctx_out,
                                                 transcode_encoder_format_in( id->encoder ),
                                                 id ) != VLC_SUCCESS )
                    goto error;
            }

            /* Store the current encoder input chroma to detect whether we need
             * a converter in p_final_conv_static. The encoder will override it
             * if it needs any different format or chroma. */
            es_format_t filter_fmt_out;
            es_format_Copy( &filter_fmt_out, transcode_encoder_format_in( id->encoder ) );
            bool is_encoder_open = transcode_encoder_opened( id->encoder );

            /* Start missing encoder */
            if( !is_encoder_open &&
                transcode_encoder_open( id->encoder, id->p_enccfg ) != VLC_SUCCESS )
            {
                msg_Err( p_stream, "cannot find video encoder (module:%s fourcc:%4.4s). "
                                   "Take a look few lines earlier to see possible reason.",
                                   id->p_enccfg->psz_name ? id->p_enccfg->psz_name : "any",
                                   (char *)&id->p_enccfg->i_codec );
                goto error;
            }

            /* The fmt_in may have been overriden by the encoder. */
            const es_format_t *encoder_fmt_in = transcode_encoder_format_in( id->encoder );

            /* In case the encoder wasn't open yet, check if we need to add
             * a converter between last user filter and encoder. */
            if( !is_encoder_open &&
                filter_fmt_out.i_codec != encoder_fmt_in->i_codec )
            {
                if ( !id->p_final_conv_static )
                    id->p_final_conv_static =
                        filter_chain_NewVideo( p_stream, false, NULL );
                filter_chain_Reset( id->p_final_conv_static,
                                    &filter_fmt_out,
                                    //encoder_vctx_in,
                                    NULL,
                                    encoder_fmt_in );
                filter_chain_AppendConverter( id->p_final_conv_static, NULL );
            }
            es_format_Clean(&filter_fmt_out);

            msg_Dbg( p_stream, "destination (after video filters) %ux%u",
                               transcode_encoder_format_in( id->encoder )->video.i_width,
                               transcode_encoder_format_in( id->encoder )->video.i_height );

            if( !id->downstream_id )
                id->downstream_id =
                    id->pf_transcode_downstream_add( p_stream,
                                                     &id->p_decoder->fmt_in,
                                                     transcode_encoder_format_out( id->encoder ) );
            if( !id->downstream_id )
            {
                msg_Err( p_stream, "cannot output transcoded stream %4.4s",
                                   (char *) &id->p_enccfg->i_codec );
                goto error;
            }
        }

        /* Run the filter and output chains; first with the picture,
         * and then with NULL as many times as we need until they
         * stop outputting frames.
         */
        for ( picture_t *p_in = p_pic; ; p_in = NULL /* drain second time */ )
        {
            /* Run filter chain */
            filter_chain_t * primary_chains[] = { id->p_f_chain,
                                                  id->p_conv_nonstatic,
                                                  id->p_conv_static };
            for( size_t i=0; p_in && i<ARRAY_SIZE(primary_chains); i++ )
            {
                if( !primary_chains[i] )
                    continue;
                p_in = filter_chain_VideoFilter( primary_chains[i], p_in );
            }

            if( !p_in )
                break;

            for ( ;; p_in = NULL /* drain second time */ )
            {
                /* Run user specified filter chain */
                filter_chain_t * secondary_chains[] = { id->p_uf_chain,
                                                        id->p_final_conv_static };
                for( size_t i=0; p_in && i<ARRAY_SIZE(secondary_chains); i++ )
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
                    block_t *p_encoded = transcode_encoder_encode( id->encoder, p_in );
                    if( p_encoded )
                        block_ChainAppend( out, p_encoded );
                    picture_Release( p_in );
                }
            }
        }

        if( b_eos )
        {
            msg_Info( p_stream, "Drain/restart on EOS" );
            if( transcode_encoder_drain( id->encoder, out ) != VLC_SUCCESS )
                goto error;
            transcode_encoder_close( id->encoder );
            /* Close filters */
            transcode_remove_filters( &id->p_f_chain );
            transcode_remove_filters( &id->p_conv_nonstatic );
            transcode_remove_filters( &id->p_conv_static );
            transcode_remove_filters( &id->p_uf_chain );
            transcode_remove_filters( &id->p_final_conv_static );
            tag_last_block_with_flag( out, BLOCK_FLAG_END_OF_SEQUENCE );
            b_eos = false;
        }

        continue;
error:
        if( p_pic )
            picture_Release( p_pic );
        id->b_error = true;
    } while( p_pics );

    if( id->p_enccfg->video.threads.i_count >= 1 )
    {
        /* Pick up any return data the encoder thread wants to output. */
        block_ChainAppend( out, transcode_encoder_get_output_async( id->encoder ) );
    }

    /* Drain encoder */
    if( unlikely( !id->b_error && in == NULL ) && transcode_encoder_opened( id->encoder ) )
    {
        msg_Dbg( p_stream, "Flushing thread and waiting that");
        if( transcode_encoder_drain( id->encoder, out ) == VLC_SUCCESS )
            msg_Dbg( p_stream, "Flushing done");
        else
            msg_Warn( p_stream, "Flushing failed");
    }

    if( b_eos )
        tag_last_block_with_flag( out, BLOCK_FLAG_END_OF_SEQUENCE );

    return id->b_error ? VLC_EGENERIC : VLC_SUCCESS;
}
