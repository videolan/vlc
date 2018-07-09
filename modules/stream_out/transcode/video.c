/*****************************************************************************
 * video.c: transcoding stream output module (video)
 *****************************************************************************
 * Copyright (C) 2003-2009 VLC authors and VideoLAN
 * $Id$
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

static const video_format_t* filtered_video_format( sout_stream_id_sys_t *id,
                                                  picture_t *p_pic )
{
    assert( id && p_pic );
    if( id->p_uf_chain )
        return &filter_chain_GetFmtOut( id->p_uf_chain )->video;
    else if( id->p_f_chain )
        return &filter_chain_GetFmtOut( id->p_f_chain )->video;
    else
        return &p_pic->format;
}

static int video_update_format_decoder( decoder_t *p_dec )
{
    struct decoder_owner *p_owner = dec_get_owner( p_dec );
    sout_stream_id_sys_t *id = p_owner->id;
    vlc_object_t        *p_obj = p_owner->p_obj;
    filter_chain_t       *test_chain;

    filter_owner_t filter_owner = {
        .sys = id,
    };

    vlc_mutex_lock( &id->fifo.lock );

    const es_format_t *p_enc_in = transcode_encoder_format_in( id->encoder );

    if( p_enc_in->i_codec == p_dec->fmt_out.i_codec ||
        video_format_IsSimilar( &id->decoder_out.video, &p_dec->fmt_out.video ) )
    {
        vlc_mutex_unlock( &id->fifo.lock );
        return 0;
    }

    video_format_Clean( &id->decoder_out.video );
    video_format_Copy( &id->decoder_out.video, &p_dec->fmt_out.video );

    /* crap, decoders resetting the whole fmtout... */
    es_format_SetMeta( &id->decoder_out, &p_dec->fmt_in );

    vlc_mutex_unlock( &id->fifo.lock );

    msg_Dbg( p_obj, "Checking if filter chain %4.4s -> %4.4s is possible",
                 (char *)&p_dec->fmt_out.i_codec, (char*)&p_enc_in->i_codec );
    test_chain = filter_chain_NewVideo( p_obj, false, &filter_owner );
    filter_chain_Reset( test_chain, &p_dec->fmt_out, &p_dec->fmt_out );

    int chain_works = filter_chain_AppendConverter( test_chain, &p_dec->fmt_out, p_enc_in );
    filter_chain_Delete( test_chain );

    msg_Dbg( p_obj, "Filter chain testing done, input chroma %4.4s seems to be %s for transcode",
                     (char *)&p_dec->fmt_out.video.i_chroma,
                     chain_works == 0 ? "possible" : "not possible");
    return chain_works;
}

static picture_t *video_new_buffer_decoder( decoder_t *p_dec )
{
    return picture_NewFromFormat( &p_dec->fmt_out.video );
}

static picture_t *video_new_buffer_encoder( transcode_encoder_t *p_enc )
{
    return picture_NewFromFormat( &transcode_encoder_format_in( p_enc )->video );
}

static picture_t *transcode_video_filter_buffer_new( filter_t *p_filter )
{
    p_filter->fmt_out.video.i_chroma = p_filter->fmt_out.i_codec;
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

    /* Open decoder
     */
    dec_get_owner( id->p_decoder )->id = id;

    static const struct decoder_owner_callbacks dec_cbs =
    {
        .video = {
            video_update_format_decoder,
            video_new_buffer_decoder,
            decoder_queue_video,
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
    video_format_Init( &id->fmt_input_video, 0 );

    if( id->decoder_out.i_codec == 0 ) /* format_update can happen on open() */
    {
        es_format_Clean( &id->decoder_out );
        es_format_Copy( &id->decoder_out, &id->p_decoder->fmt_out );
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

    if( transcode_encoder_test( VLC_OBJECT(p_stream),
                                id->p_enccfg,
                                &id->p_decoder->fmt_in,
                                id->p_decoder->fmt_out.i_codec,
                                &encoder_tested_fmt_in ) )
    {
        module_unneed( id->p_decoder, id->p_decoder->p_module );
        id->p_decoder->p_module = NULL;
        video_format_Clean( &id->fmt_input_video );
        es_format_Clean( &id->decoder_out );
        es_format_Clean( &encoder_tested_fmt_in );
        return VLC_EGENERIC;
    }

    id->encoder = transcode_encoder_new( VLC_OBJECT(p_stream), &encoder_tested_fmt_in );
    if( !id->encoder )
    {
        module_unneed( id->p_decoder, id->p_decoder->p_module );
        id->p_decoder->p_module = NULL;
        video_format_Clean( &id->fmt_input_video );
        es_format_Clean( &encoder_tested_fmt_in );
        es_format_Clean( &id->decoder_out );
        return VLC_EGENERIC;
    }
    /* Will use this format as encoder input for now */
    transcode_encoder_update_format_in( id->encoder, &encoder_tested_fmt_in );

    es_format_Clean( &encoder_tested_fmt_in );

    return VLC_SUCCESS;
}

static const struct filter_video_callbacks transcode_filter_video_cbs =
{
    .buffer_new = transcode_video_filter_buffer_new,
};

static void transcode_video_filter_init( sout_stream_t *p_stream,
                                         const sout_filters_config_t *p_cfg,
                                         bool b_master_sync,
                                         sout_stream_id_sys_t *id )
{
    const es_format_t *p_src = &id->p_decoder->fmt_out;
    const es_format_t *p_dst = transcode_encoder_format_in( id->encoder );

    /* Build chain */
    filter_owner_t owner = {
        .video = &transcode_filter_video_cbs,
        .sys = id,
    };
    id->p_f_chain = filter_chain_NewVideo( p_stream, false, &owner );
    filter_chain_Reset( id->p_f_chain, p_src, p_src );

    /* Deinterlace */
    if( p_cfg->video.psz_deinterlace != NULL )
    {
        filter_chain_AppendFilter( id->p_f_chain,
                                   p_cfg->video.psz_deinterlace,
                                   p_cfg->video.p_deinterlace_cfg,
                                   p_src, p_src );
        p_src = filter_chain_GetFmtOut( id->p_f_chain );
    }

    /* SPU Sources */
    if( p_cfg->video.psz_spu_sources )
    {
        if( id->p_spu || (id->p_spu = spu_Create( p_stream, NULL )) )
            spu_ChangeSources( id->p_spu, p_cfg->video.psz_spu_sources );
    }

    if( b_master_sync )
    {
        filter_chain_AppendFilter( id->p_f_chain, "fps", NULL, p_src, p_dst );
        p_src = filter_chain_GetFmtOut( id->p_f_chain );
    }

    if( p_cfg->psz_filters )
    {
        id->p_uf_chain = filter_chain_NewVideo( p_stream, true, &owner );
        filter_chain_Reset( id->p_uf_chain, p_src, p_dst );
        if( p_src->video.i_chroma != p_dst->video.i_chroma )
        {
            filter_chain_AppendConverter( id->p_uf_chain, p_src, p_dst );
        }
        filter_chain_AppendFromString( id->p_uf_chain, p_cfg->psz_filters );
        p_src = filter_chain_GetFmtOut( id->p_uf_chain );

        /* Update encoder so it matches filters output */
        transcode_encoder_update_format_in( id->encoder, p_src );

        /* FIXME: modifying decoder output size from filters
         *        sounds really suspicious an buggy
         *        see also size adaption already done in conversion_video_filter_append() */
        const es_format_t *enc_out = transcode_encoder_format_out( id->encoder );
        if( enc_out->video.i_width != p_dst->video.i_width ||
            enc_out->video.i_height != p_dst->video.i_height ||
            enc_out->video.i_sar_num != p_dst->video.i_sar_num ||
            enc_out->video.i_sar_den != p_dst->video.i_sar_den )
        {
            es_format_t tmp;
            es_format_Copy( &tmp, enc_out );
            tmp.video.i_width = p_dst->video.i_width;
            tmp.video.i_height = p_dst->video.i_height;
            tmp.video.i_sar_num = p_dst->video.i_sar_num;
            tmp.video.i_sar_den = p_dst->video.i_sar_den;
            transcode_encoder_update_format_in( id->encoder, &tmp );
            es_format_Clean( &tmp );
        }
    }
}

/* Take care of the scaling and chroma conversions. */
static int conversion_video_filter_append( sout_stream_id_sys_t *id,
                                           picture_t *p_pic )
{
    const video_format_t *p_src = filtered_video_format( id, p_pic );

    const es_format_t *p_enc_in = transcode_encoder_format_in( id->encoder );

    if( ( p_src->i_chroma != p_enc_in->video.i_chroma ) ||
        ( p_src->i_width != p_enc_in->video.i_width ) ||
        ( p_src->i_height != p_enc_in->video.i_height ) )
    {
        es_format_t fmt_out;
        es_format_Init( &fmt_out, VIDEO_ES, p_src->i_chroma );
        fmt_out.video = *p_src;
        return filter_chain_AppendConverter( id->p_uf_chain ? id->p_uf_chain : id->p_f_chain,
                                             &fmt_out, p_enc_in );
    }
    return VLC_SUCCESS;
}


void transcode_video_clean( sout_stream_t *p_stream,
                                   sout_stream_id_sys_t *id )
{
    VLC_UNUSED(p_stream);

    /* Close decoder */
    if( id->p_decoder->p_module )
        module_unneed( id->p_decoder, id->p_decoder->p_module );
    if( id->p_decoder->p_description )
        vlc_meta_Delete( id->p_decoder->p_description );

    /* Close encoder */
    transcode_encoder_close( id->encoder );
    transcode_encoder_delete( id->encoder );

    video_format_Clean( &id->fmt_input_video );
    es_format_Clean( &id->decoder_out );

    /* Close filters */
    if( id->p_f_chain )
        filter_chain_Delete( id->p_f_chain );
    if( id->p_uf_chain )
        filter_chain_Delete( id->p_uf_chain );
    if( id->p_spu_blender )
        filter_DeleteBlend( id->p_spu_blender );
    if( id->p_spu )
        spu_Destroy( id->p_spu );
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

int transcode_video_get_output_dimensions( sout_stream_t *p_stream, sout_stream_id_sys_t *id,
                                           unsigned *w, unsigned *h )
{
    VLC_UNUSED(p_stream);
    vlc_mutex_lock( &id->fifo.lock );
    *w = id->fmt_input_video.i_visible_width;
    *h = id->fmt_input_video.i_visible_height;
    if( !*w || !*h )
    {
        *w = id->decoder_out.video.i_visible_width;
        *h = id->decoder_out.video.i_visible_height;
    }
    vlc_mutex_unlock( &id->fifo.lock );
    return (*w && *h) ? VLC_SUCCESS : VLC_EGENERIC;
}

static picture_t * RenderSubpictures( sout_stream_t *p_stream, sout_stream_id_sys_t *id,
                                       picture_t *p_pic )
{
    VLC_UNUSED(p_stream);

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
                                         &outfmt,
                                         p_pic->date, p_pic->date, false );

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

int transcode_video_process( sout_stream_t *p_stream, sout_stream_id_sys_t *id,
                                    block_t *in, block_t **out )
{
    *out = NULL;

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
              !video_format_IsSimilar( &id->fmt_input_video, &p_pic->format ) ) )
        {
            if( !transcode_encoder_opened(id->encoder) ) /* Configure Encoder input/output */
            {
                transcode_encoder_video_configure( VLC_OBJECT(p_stream),
                                                   &id->p_decoder->fmt_in.video,
                                                   &id->p_decoder->fmt_out.video,
                                                   id->p_enccfg,
                                                   filtered_video_format( id, p_pic ),
                                                   id->encoder );
                /* will be opened below */
            }
            else /* picture format has changed */
            {
                msg_Info( p_stream, "aspect-ratio changed, reiniting. %i -> %i : %i -> %i.",
                            id->fmt_input_video.i_sar_num, p_pic->format.i_sar_num,
                            id->fmt_input_video.i_sar_den, p_pic->format.i_sar_den
                        );
                /* Close filters, encoder format input can't change */
                if( id->p_f_chain )
                    filter_chain_Delete( id->p_f_chain );
                id->p_f_chain = NULL;
                if( id->p_uf_chain )
                    filter_chain_Delete( id->p_uf_chain );
                id->p_uf_chain = NULL;
                if( id->p_spu_blender )
                    filter_DeleteBlend( id->p_spu_blender );
                id->p_spu_blender = NULL;

                video_format_Clean( &id->fmt_input_video );
            }

            video_format_Copy( &id->fmt_input_video, &p_pic->format );

            transcode_video_filter_init( p_stream, id->p_filterscfg,
                                         (id->p_enccfg->video.fps.num > 0), id );
            if( conversion_video_filter_append( id, p_pic ) != VLC_SUCCESS )
                goto error;

            /* Start missing encoder */
            if( !transcode_encoder_opened( id->encoder ) &&
                transcode_encoder_open( id->encoder, id->p_enccfg ) != VLC_SUCCESS )
            {
                msg_Err( p_stream, "cannot find audio encoder (module:%s fourcc:%4.4s). "
                                   "Take a look few lines earlier to see possible reason.",
                                   id->p_enccfg->psz_name ? id->p_enccfg->psz_name : "any",
                                   (char *)&id->p_enccfg->i_codec );
                goto error;
            }

            msg_Dbg( p_stream, "destination (after video filters) %ux%u",
                               transcode_encoder_format_in( id->encoder )->video.i_width,
                               transcode_encoder_format_in( id->encoder )->video.i_height );

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
            if( id->p_f_chain )
                p_in = filter_chain_VideoFilter( id->p_f_chain, p_in );

            if( !p_in )
                break;

            for ( ;; p_in = NULL /* drain second time */ )
            {
                /* Run user specified filter chain */
                if( id->p_uf_chain )
                    p_in = filter_chain_VideoFilter( id->p_uf_chain, p_in );

                if( !p_in )
                    break;

                /* Blend subpictures */
                p_in = RenderSubpictures( p_stream, id, p_in );

                if( p_in )
                {
                    block_t *p_encoded = transcode_encoder_encode( id->encoder, p_in );
                    if( p_encoded )
                        block_ChainAppend( out, p_encoded );
                    picture_Release( p_in );
                }
            }
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
        if( id->p_enccfg->video.threads.i_count == 0 )
        {
            block_t *p_block;
            do {
                p_block = transcode_encoder_encode( id->encoder, NULL );
                block_ChainAppend( out, p_block );
            } while( p_block );
        }
        else
        {
            msg_Dbg( p_stream, "Flushing thread and waiting that");
            transcode_encoder_close( id->encoder );
            block_ChainAppend( out, transcode_encoder_get_output_async( id->encoder ) );
            msg_Dbg( p_stream, "Flushing done");
        }
    }

    return id->b_error ? VLC_EGENERIC : VLC_SUCCESS;
}
