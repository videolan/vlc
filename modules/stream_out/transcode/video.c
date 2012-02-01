/*****************************************************************************
 * video.c: transcoding stream output module (video)
 *****************************************************************************
 * Copyright (C) 2003-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Jean-Paul Saman <jpsaman #_at_# m2x dot nl>
 *          Antoine Cellerier <dionoea at videolan dot org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#include "transcode.h"

#include <vlc_meta.h>
#include <vlc_spu.h>
#include <vlc_modules.h>

#define ENC_FRAMERATE (25 * 1000 + .5)
#define ENC_FRAMERATE_BASE 1000

struct decoder_owner_sys_t
{
    sout_stream_sys_t *p_sys;
};

static void video_del_buffer_decoder( decoder_t *p_decoder, picture_t *p_pic )
{
    VLC_UNUSED(p_decoder);
    picture_Release( p_pic );
}

static void video_link_picture_decoder( decoder_t *p_dec, picture_t *p_pic )
{
    VLC_UNUSED(p_dec);
    picture_Hold( p_pic );
}

static void video_unlink_picture_decoder( decoder_t *p_dec, picture_t *p_pic )
{
    VLC_UNUSED(p_dec);
    picture_Release( p_pic );
}

static picture_t *video_new_buffer_decoder( decoder_t *p_dec )
{
    sout_stream_sys_t *p_ssys = p_dec->p_owner->p_sys;
    if( p_ssys->i_threads >= 1 )
    {
        int i_first_pic = p_ssys->i_first_pic;

        if( p_ssys->i_first_pic != p_ssys->i_last_pic )
        {
            /* Encoder still has stuff to encode, wait to clear-up the list */
            while( p_ssys->i_first_pic == i_first_pic )
            {
#warning THERE IS DEFINITELY A BUG! LOCKING IS INSUFFICIENT!
                msleep( 10000 );
                barrier ();
            }
        }
    }

    p_dec->fmt_out.video.i_chroma = p_dec->fmt_out.i_codec;
    return picture_NewFromFormat( &p_dec->fmt_out.video );
}

static picture_t *transcode_video_filter_buffer_new( filter_t *p_filter )
{
    p_filter->fmt_out.video.i_chroma = p_filter->fmt_out.i_codec;
    return picture_NewFromFormat( &p_filter->fmt_out.video );
}
static void transcode_video_filter_buffer_del( filter_t *p_filter, picture_t *p_pic )
{
    VLC_UNUSED(p_filter);
    picture_Release( p_pic );
}

static int transcode_video_filter_allocation_init( filter_t *p_filter,
                                                   void *p_data )
{
    VLC_UNUSED(p_data);
    p_filter->pf_video_buffer_new = transcode_video_filter_buffer_new;
    p_filter->pf_video_buffer_del = transcode_video_filter_buffer_del;
    return VLC_SUCCESS;
}

static void transcode_video_filter_allocation_clear( filter_t *p_filter )
{
    VLC_UNUSED(p_filter);
}

static void* EncoderThread( void *obj )
{
    sout_stream_sys_t *p_sys = (sout_stream_sys_t*)obj;
    sout_stream_id_t *id = p_sys->id_video;
    picture_t *p_pic;
    int canc = vlc_savecancel ();

    for( ;; )
    {
        block_t *p_block;

        vlc_mutex_lock( &p_sys->lock_out );
        while( !p_sys->b_abort && p_sys->i_last_pic == p_sys->i_first_pic )
            vlc_cond_wait( &p_sys->cond, &p_sys->lock_out );

        if( p_sys->b_abort )
        {
            vlc_mutex_unlock( &p_sys->lock_out );
            break;
        }

        p_pic = p_sys->pp_pics[p_sys->i_first_pic++];
        p_sys->i_first_pic %= PICTURE_RING_SIZE;
        vlc_mutex_unlock( &p_sys->lock_out );

        p_block = id->p_encoder->pf_encode_video( id->p_encoder, p_pic );

        vlc_mutex_lock( &p_sys->lock_out );
        block_ChainAppend( &p_sys->p_buffers, p_block );

        vlc_mutex_unlock( &p_sys->lock_out );
        picture_Release( p_pic );
    }

    while( p_sys->i_last_pic != p_sys->i_first_pic )
    {
        p_pic = p_sys->pp_pics[p_sys->i_first_pic++];
        p_sys->i_first_pic %= PICTURE_RING_SIZE;
        picture_Release( p_pic );
    }
    block_ChainRelease( p_sys->p_buffers );

    vlc_restorecancel (canc);
    return NULL;
}

int transcode_video_new( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    /* Open decoder
     * Initialization of decoder structures
     */
    id->p_decoder->fmt_out = id->p_decoder->fmt_in;
    id->p_decoder->fmt_out.i_extra = 0;
    id->p_decoder->fmt_out.p_extra = 0;
    id->p_decoder->pf_decode_video = NULL;
    id->p_decoder->pf_get_cc = NULL;
    id->p_decoder->pf_get_cc = 0;
    id->p_decoder->pf_vout_buffer_new = video_new_buffer_decoder;
    id->p_decoder->pf_vout_buffer_del = video_del_buffer_decoder;
    id->p_decoder->pf_picture_link    = video_link_picture_decoder;
    id->p_decoder->pf_picture_unlink  = video_unlink_picture_decoder;
    id->p_decoder->p_owner = malloc( sizeof(decoder_owner_sys_t) );
    if( !id->p_decoder->p_owner )
        return VLC_EGENERIC;

    id->p_decoder->p_owner->p_sys = p_sys;
    /* id->p_decoder->p_cfg = p_sys->p_video_cfg; */

    id->p_decoder->p_module =
        module_need( id->p_decoder, "decoder", "$codec", false );

    if( !id->p_decoder->p_module )
    {
        msg_Err( p_stream, "cannot find video decoder" );
        free( id->p_decoder->p_owner );
        return VLC_EGENERIC;
    }

    /*
     * Open encoder.
     * Because some info about the decoded input will only be available
     * once the first frame is decoded, we actually only test the availability
     * of the encoder here.
     */

    /* Initialization of encoder format structures */
    es_format_Init( &id->p_encoder->fmt_in, id->p_decoder->fmt_in.i_cat,
                    id->p_decoder->fmt_out.i_codec );
    id->p_encoder->fmt_in.video.i_chroma = id->p_decoder->fmt_out.i_codec;

    /* The dimensions will be set properly later on.
     * Just put sensible values so we can test an encoder is available. */
    id->p_encoder->fmt_in.video.i_width =
        id->p_encoder->fmt_out.video.i_width
          ? id->p_encoder->fmt_out.video.i_width
          : id->p_decoder->fmt_in.video.i_width
            ? id->p_decoder->fmt_in.video.i_width : 16;
    id->p_encoder->fmt_in.video.i_height =
        id->p_encoder->fmt_out.video.i_height
          ? id->p_encoder->fmt_out.video.i_height
          : id->p_decoder->fmt_in.video.i_height
            ? id->p_decoder->fmt_in.video.i_height : 16;
    id->p_encoder->fmt_in.video.i_frame_rate = ENC_FRAMERATE;
    id->p_encoder->fmt_in.video.i_frame_rate_base = ENC_FRAMERATE_BASE;

    id->p_encoder->i_threads = p_sys->i_threads;
    id->p_encoder->p_cfg = p_sys->p_video_cfg;

    id->p_encoder->p_module =
        module_need( id->p_encoder, "encoder", p_sys->psz_venc, true );
    if( !id->p_encoder->p_module )
    {
        msg_Err( p_stream, "cannot find video encoder (module:%s fourcc:%4.4s). Take a look few lines earlier to see possible reason.",
                 p_sys->psz_venc ? p_sys->psz_venc : "any",
                 (char *)&p_sys->i_vcodec );
        module_unneed( id->p_decoder, id->p_decoder->p_module );
        id->p_decoder->p_module = 0;
        free( id->p_decoder->p_owner );
        return VLC_EGENERIC;
    }

    /* Close the encoder.
     * We'll open it only when we have the first frame. */
    module_unneed( id->p_encoder, id->p_encoder->p_module );
    if( id->p_encoder->fmt_out.p_extra )
    {
        free( id->p_encoder->fmt_out.p_extra );
        id->p_encoder->fmt_out.p_extra = NULL;
        id->p_encoder->fmt_out.i_extra = 0;
    }
    id->p_encoder->p_module = NULL;

    if( p_sys->i_threads >= 1 )
    {
        int i_priority = p_sys->b_high_priority ? VLC_THREAD_PRIORITY_OUTPUT :
                           VLC_THREAD_PRIORITY_VIDEO;
        p_sys->id_video = id;
        vlc_mutex_init( &p_sys->lock_out );
        vlc_cond_init( &p_sys->cond );
        memset( p_sys->pp_pics, 0, sizeof(p_sys->pp_pics) );
        p_sys->i_first_pic = 0;
        p_sys->i_last_pic = 0;
        p_sys->p_buffers = NULL;
        p_sys->b_abort = 0;
        if( vlc_clone( &p_sys->thread, EncoderThread, p_sys, i_priority ) )
        {
            msg_Err( p_stream, "cannot spawn encoder thread" );
            module_unneed( id->p_decoder, id->p_decoder->p_module );
            id->p_decoder->p_module = 0;
            free( id->p_decoder->p_owner );
            return VLC_EGENERIC;
        }
    }
    return VLC_SUCCESS;
}

static void transcode_video_filter_init( sout_stream_t *p_stream,
                                         sout_stream_id_t *id )
{

    id->p_f_chain = filter_chain_New( p_stream, "video filter2",
                                     false,
                                     transcode_video_filter_allocation_init,
                                     transcode_video_filter_allocation_clear,
                                     p_stream->p_sys );
    /* Deinterlace */
    if( p_stream->p_sys->b_deinterlace )
    {
       filter_chain_AppendFilter( id->p_f_chain,
                                  p_stream->p_sys->psz_deinterlace,
                                  p_stream->p_sys->p_deinterlace_cfg,
                                  &id->p_decoder->fmt_out,
                                  &id->p_decoder->fmt_out );
    }

    /* Take care of the scaling and chroma conversions */
    if( ( id->p_decoder->fmt_out.video.i_chroma !=
          id->p_encoder->fmt_in.video.i_chroma ) ||
        ( id->p_decoder->fmt_out.video.i_width !=
          id->p_encoder->fmt_in.video.i_width ) ||
        ( id->p_decoder->fmt_out.video.i_height !=
          id->p_encoder->fmt_in.video.i_height ) )
    {
       filter_chain_AppendFilter( id->p_f_chain,
                                  NULL, NULL,
                                  &id->p_decoder->fmt_out,
                                  &id->p_encoder->fmt_in );
    }

    if( p_stream->p_sys->psz_vf2 )
    {
        const es_format_t *p_fmt_out;
        id->p_uf_chain = filter_chain_New( p_stream, "video filter2",
                                          true,
                           transcode_video_filter_allocation_init,
                           transcode_video_filter_allocation_clear,
                           p_stream->p_sys );
        filter_chain_Reset( id->p_uf_chain, &id->p_encoder->fmt_in,
                            &id->p_encoder->fmt_in );
        filter_chain_AppendFromString( id->p_uf_chain, p_stream->p_sys->psz_vf2 );
        p_fmt_out = filter_chain_GetFmtOut( id->p_uf_chain );
        es_format_Copy( &id->p_encoder->fmt_in, p_fmt_out );
        id->p_encoder->fmt_out.video.i_width =
            id->p_encoder->fmt_in.video.i_width;
        id->p_encoder->fmt_out.video.i_height =
            id->p_encoder->fmt_in.video.i_height;
        id->p_encoder->fmt_out.video.i_sar_num =
            id->p_encoder->fmt_in.video.i_sar_num;
        id->p_encoder->fmt_out.video.i_sar_den =
            id->p_encoder->fmt_in.video.i_sar_den;
    }

}

static void transcode_video_encoder_init( sout_stream_t *p_stream,
                                          sout_stream_id_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    /* Calculate scaling
     * width/height of source */
    int i_src_width = id->p_decoder->fmt_out.video.i_width;
    int i_src_height = id->p_decoder->fmt_out.video.i_height;

    /* with/height scaling */
    float f_scale_width = 1;
    float f_scale_height = 1;

    /* width/height of output stream */
    int i_dst_width;
    int i_dst_height;

    /* aspect ratio */
    float f_aspect = (double)id->p_decoder->fmt_out.video.i_sar_num *
                     id->p_decoder->fmt_out.video.i_width /
                     id->p_decoder->fmt_out.video.i_sar_den /
                     id->p_decoder->fmt_out.video.i_height;

    msg_Dbg( p_stream, "decoder aspect is %f:1", f_aspect );

    /* Change f_aspect from source frame to source pixel */
    f_aspect = f_aspect * i_src_height / i_src_width;
    msg_Dbg( p_stream, "source pixel aspect is %f:1", f_aspect );

    /* Calculate scaling factor for specified parameters */
    if( id->p_encoder->fmt_out.video.i_width <= 0 &&
        id->p_encoder->fmt_out.video.i_height <= 0 && p_sys->f_scale )
    {
        /* Global scaling. Make sure width will remain a factor of 16 */
        float f_real_scale;
        int  i_new_height;
        int i_new_width = i_src_width * p_sys->f_scale;

        if( i_new_width % 16 <= 7 && i_new_width >= 16 )
            i_new_width -= i_new_width % 16;
        else
            i_new_width += 16 - i_new_width % 16;

        f_real_scale = (float)( i_new_width ) / (float) i_src_width;

        i_new_height = __MAX( 16, i_src_height * (float)f_real_scale );

        f_scale_width = f_real_scale;
        f_scale_height = (float) i_new_height / (float) i_src_height;
    }
    else if( id->p_encoder->fmt_out.video.i_width > 0 &&
             id->p_encoder->fmt_out.video.i_height <= 0 )
    {
        /* Only width specified */
        f_scale_width = (float)id->p_encoder->fmt_out.video.i_width/i_src_width;
        f_scale_height = f_scale_width;
    }
    else if( id->p_encoder->fmt_out.video.i_width <= 0 &&
             id->p_encoder->fmt_out.video.i_height > 0 )
    {
         /* Only height specified */
         f_scale_height = (float)id->p_encoder->fmt_out.video.i_height/i_src_height;
         f_scale_width = f_scale_height;
     }
     else if( id->p_encoder->fmt_out.video.i_width > 0 &&
              id->p_encoder->fmt_out.video.i_height > 0 )
     {
         /* Width and height specified */
         f_scale_width = (float)id->p_encoder->fmt_out.video.i_width/i_src_width;
         f_scale_height = (float)id->p_encoder->fmt_out.video.i_height/i_src_height;
     }

     /* check maxwidth and maxheight */
     if( p_sys->i_maxwidth && f_scale_width > (float)p_sys->i_maxwidth /
                                                     i_src_width )
     {
         f_scale_width = (float)p_sys->i_maxwidth / i_src_width;
     }

     if( p_sys->i_maxheight && f_scale_height > (float)p_sys->i_maxheight /
                                                       i_src_height )
     {
         f_scale_height = (float)p_sys->i_maxheight / i_src_height;
     }


     /* Change aspect ratio from source pixel to scaled pixel */
     f_aspect = f_aspect * f_scale_height / f_scale_width;
     msg_Dbg( p_stream, "scaled pixel aspect is %f:1", f_aspect );

     /* f_scale_width and f_scale_height are now final */
     /* Calculate width, height from scaling
      * Make sure its multiple of 2
      */
     i_dst_width =  2 * (int)(f_scale_width*i_src_width/2+0.5);
     i_dst_height = 2 * (int)(f_scale_height*i_src_height/2+0.5);

     /* Change aspect ratio from scaled pixel to output frame */
     f_aspect = f_aspect * i_dst_width / i_dst_height;

     /* Store calculated values */
     id->p_encoder->fmt_out.video.i_width =
     id->p_encoder->fmt_out.video.i_visible_width = i_dst_width;
     id->p_encoder->fmt_out.video.i_height =
     id->p_encoder->fmt_out.video.i_visible_height = i_dst_height;

     id->p_encoder->fmt_in.video.i_width =
     id->p_encoder->fmt_in.video.i_visible_width = i_dst_width;
     id->p_encoder->fmt_in.video.i_height =
     id->p_encoder->fmt_in.video.i_visible_height = i_dst_height;

     msg_Dbg( p_stream, "source %ix%i, destination %ix%i",
         i_src_width, i_src_height,
         i_dst_width, i_dst_height
     );

    /* Handle frame rate conversion */
    if( !id->p_encoder->fmt_out.video.i_frame_rate ||
        !id->p_encoder->fmt_out.video.i_frame_rate_base )
    {
        if( id->p_decoder->fmt_out.video.i_frame_rate &&
            id->p_decoder->fmt_out.video.i_frame_rate_base )
        {
            id->p_encoder->fmt_out.video.i_frame_rate =
                id->p_decoder->fmt_out.video.i_frame_rate;
            id->p_encoder->fmt_out.video.i_frame_rate_base =
                id->p_decoder->fmt_out.video.i_frame_rate_base;
        }
        else
        {
            /* Pick a sensible default value */
            id->p_encoder->fmt_out.video.i_frame_rate = ENC_FRAMERATE;
            id->p_encoder->fmt_out.video.i_frame_rate_base = ENC_FRAMERATE_BASE;
        }
    }

    id->p_encoder->fmt_in.video.i_frame_rate =
        id->p_encoder->fmt_out.video.i_frame_rate;
    id->p_encoder->fmt_in.video.i_frame_rate_base =
        id->p_encoder->fmt_out.video.i_frame_rate_base;

    date_Init( &id->interpolated_pts,
               id->p_encoder->fmt_out.video.i_frame_rate,
               id->p_encoder->fmt_out.video.i_frame_rate_base );

    /* Check whether a particular aspect ratio was requested */
    if( id->p_encoder->fmt_out.video.i_sar_num <= 0 ||
        id->p_encoder->fmt_out.video.i_sar_den <= 0 )
    {
        vlc_ureduce( &id->p_encoder->fmt_out.video.i_sar_num,
                     &id->p_encoder->fmt_out.video.i_sar_den,
                     (uint64_t)id->p_decoder->fmt_out.video.i_sar_num * i_src_width  * i_dst_height,
                     (uint64_t)id->p_decoder->fmt_out.video.i_sar_den * i_src_height * i_dst_width,
                     0 );
    }
    else
    {
        vlc_ureduce( &id->p_encoder->fmt_out.video.i_sar_num,
                     &id->p_encoder->fmt_out.video.i_sar_den,
                     id->p_encoder->fmt_out.video.i_sar_num,
                     id->p_encoder->fmt_out.video.i_sar_den,
                     0 );
    }

    id->p_encoder->fmt_in.video.i_sar_num =
        id->p_encoder->fmt_out.video.i_sar_num;
    id->p_encoder->fmt_in.video.i_sar_den =
        id->p_encoder->fmt_out.video.i_sar_den;

    msg_Dbg( p_stream, "encoder aspect is %i:%i",
             id->p_encoder->fmt_out.video.i_sar_num * id->p_encoder->fmt_out.video.i_width,
             id->p_encoder->fmt_out.video.i_sar_den * id->p_encoder->fmt_out.video.i_height );

    id->p_encoder->fmt_in.video.i_chroma = id->p_encoder->fmt_in.i_codec;
}

static int transcode_video_encoder_open( sout_stream_t *p_stream,
                                         sout_stream_id_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;


    msg_Dbg( p_stream, "destination (after video filters) %ix%i",
             id->p_encoder->fmt_in.video.i_width,
             id->p_encoder->fmt_in.video.i_height );

    id->p_encoder->p_module =
        module_need( id->p_encoder, "encoder", p_sys->psz_venc, true );
    if( !id->p_encoder->p_module )
    {
        msg_Err( p_stream, "cannot find video encoder (module:%s fourcc:%4.4s)",
                 p_sys->psz_venc ? p_sys->psz_venc : "any",
                 (char *)&p_sys->i_vcodec );
        return VLC_EGENERIC;
    }

    id->p_encoder->fmt_in.video.i_chroma = id->p_encoder->fmt_in.i_codec;

    /*  */
    id->p_encoder->fmt_out.i_codec =
        vlc_fourcc_GetCodec( VIDEO_ES, id->p_encoder->fmt_out.i_codec );

    id->id = sout_StreamIdAdd( p_stream->p_next, &id->p_encoder->fmt_out );
    if( !id->id )
    {
        msg_Err( p_stream, "cannot add this stream" );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

void transcode_video_close( sout_stream_t *p_stream,
                                   sout_stream_id_t *id )
{
    if( p_stream->p_sys->i_threads >= 1 )
    {
        vlc_mutex_lock( &p_stream->p_sys->lock_out );
        p_stream->p_sys->b_abort = true;
        vlc_cond_signal( &p_stream->p_sys->cond );
        vlc_mutex_unlock( &p_stream->p_sys->lock_out );

        vlc_join( p_stream->p_sys->thread, NULL );
        vlc_mutex_destroy( &p_stream->p_sys->lock_out );
        vlc_cond_destroy( &p_stream->p_sys->cond );
    }

    /* Close decoder */
    if( id->p_decoder->p_module )
        module_unneed( id->p_decoder, id->p_decoder->p_module );
    if( id->p_decoder->p_description )
        vlc_meta_Delete( id->p_decoder->p_description );

    free( id->p_decoder->p_owner );

    /* Close encoder */
    if( id->p_encoder->p_module )
        module_unneed( id->p_encoder, id->p_encoder->p_module );

    /* Close filters */
    if( id->p_f_chain )
        filter_chain_Delete( id->p_f_chain );
    if( id->p_uf_chain )
        filter_chain_Delete( id->p_uf_chain );
}

int transcode_video_process( sout_stream_t *p_stream, sout_stream_id_t *id,
                                    block_t *in, block_t **out )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    bool b_need_duplicate = false;
    picture_t *p_pic, *p_pic2 = NULL;
    *out = NULL;

    if( unlikely( in == NULL ) )
    {
        if( p_sys->i_threads == 0 )
        {
            block_t *p_block;
            do {
                p_block = id->p_encoder->pf_encode_video(id->p_encoder, NULL );
                block_ChainAppend( out, p_block );
            } while( p_block );
        }
        else
        {
            /*
             * FIXME: we need EncoderThread() to flush buffers and signal us
             * when it's done so we can send the last frames to the chain
             */
        }
        return VLC_SUCCESS;
    }


    while( (p_pic = id->p_decoder->pf_decode_video( id->p_decoder, &in )) )
    {

        if( p_stream->p_sout->i_out_pace_nocontrol && p_sys->b_hurry_up )
        {
            mtime_t current_date = mdate();
            if( unlikely( current_date + 50000 > p_pic->date ) )
            {
                msg_Dbg( p_stream, "late picture skipped (%"PRId64")",
                         current_date + 50000 - p_pic->date );
                picture_Release( p_pic );
                continue;
            }
        }

        if( p_sys->b_master_sync )
        {
            mtime_t i_video_drift;
            mtime_t i_master_drift = p_sys->i_master_drift;
            mtime_t i_pts;

            i_pts = date_Get( &id->interpolated_pts ) + 1;
            if ( unlikely( p_pic->date - i_pts > MASTER_SYNC_MAX_DRIFT
                  || p_pic->date - i_pts < -MASTER_SYNC_MAX_DRIFT ) )
            {
                msg_Dbg( p_stream, "drift is too high, resetting master sync" );
                date_Set( &id->interpolated_pts, p_pic->date );
                i_pts = p_pic->date + 1;
            }
            i_video_drift = p_pic->date - i_pts;
            b_need_duplicate = false;

            /* Set the pts of the frame being encoded */
            p_pic->date = i_pts;

            if( unlikely( i_video_drift < (i_master_drift - 50000) ) )
            {
#if 0
                msg_Dbg( p_stream, "dropping frame (%i)",
                         (int)(i_video_drift - i_master_drift) );
#endif
                picture_Release( p_pic );
                continue;
            }
            else if( unlikely( i_video_drift > (i_master_drift + 50000) ) )
            {
#if 0
                msg_Dbg( p_stream, "adding frame (%i)",
                         (int)(i_video_drift - i_master_drift) );
#endif
                b_need_duplicate = true;
            }
        }

        if( unlikely( !id->p_encoder->p_module ) )
        {
            transcode_video_encoder_init( p_stream, id );

            transcode_video_filter_init( p_stream, id );

            if( transcode_video_encoder_open( p_stream, id ) != VLC_SUCCESS )
            {
                picture_Release( p_pic );
                transcode_video_close( p_stream, id );
                id->b_transcode = false;
                return VLC_EGENERIC;
            }
        }

        /* Run filter chain */
        if( id->p_f_chain )
            p_pic = filter_chain_VideoFilter( id->p_f_chain, p_pic );

        /* Run user specified filter chain */
        if( id->p_uf_chain )
            p_pic = filter_chain_VideoFilter( id->p_uf_chain, p_pic );

        /*
         * Encoding
         */

        /* Check if we have a subpicture to overlay */
        if( p_sys->p_spu )
        {
            video_format_t fmt = id->p_encoder->fmt_in.video;
            if( fmt.i_visible_width <= 0 || fmt.i_visible_height <= 0 )
            {
                fmt.i_visible_width  = fmt.i_width;
                fmt.i_visible_height = fmt.i_height;
                fmt.i_x_offset       = 0;
                fmt.i_y_offset       = 0;
            }

            subpicture_t *p_subpic = spu_Render( p_sys->p_spu, NULL, &fmt, &fmt,
                                                 p_pic->date, p_pic->date, false );

            /* Overlay subpicture */
            if( p_subpic )
            {
                if( picture_IsReferenced( p_pic ) && !filter_chain_GetLength( id->p_f_chain ) )
                {
                    /* We can't modify the picture, we need to duplicate it */
                    picture_t *p_tmp = video_new_buffer_decoder( id->p_decoder );
                    if( likely( p_tmp ) )
                    {
                        picture_Copy( p_tmp, p_pic );
                        picture_Release( p_pic );
                        p_pic = p_tmp;
                    }
                }
                if( unlikely( !p_sys->p_spu_blend ) )
                    p_sys->p_spu_blend = filter_NewBlend( VLC_OBJECT( p_sys->p_spu ), &fmt );
                if( likely( p_sys->p_spu_blend ) )
                    picture_BlendSubpicture( p_pic, p_sys->p_spu_blend, p_subpic );
                subpicture_Delete( p_subpic );
            }
        }

        if( p_sys->i_threads == 0 )
        {
            block_t *p_block;

            p_block = id->p_encoder->pf_encode_video( id->p_encoder, p_pic );
            block_ChainAppend( out, p_block );
        }

        if( p_sys->b_master_sync )
        {
            mtime_t i_pts = date_Get( &id->interpolated_pts ) + 1;
            if (unlikely ( p_pic->date - i_pts > MASTER_SYNC_MAX_DRIFT
                  || p_pic->date - i_pts < -MASTER_SYNC_MAX_DRIFT ) )
            {
                msg_Dbg( p_stream, "drift is too high, resetting master sync" );
                date_Set( &id->interpolated_pts, p_pic->date );
                i_pts = p_pic->date + 1;
            }
            date_Increment( &id->interpolated_pts, 1 );

            if( unlikely( b_need_duplicate ) )
            {

               if( p_sys->i_threads >= 1 )
               {
                   /* We can't modify the picture, we need to duplicate it */
                   p_pic2 = video_new_buffer_decoder( id->p_decoder );
                   if( unlikely( p_pic2 != NULL ) )
                   {
                       picture_Copy( p_pic2, p_pic );
                       p_pic2->date = i_pts;
                   }
               }
               else
               {
                   block_t *p_block;
                   p_pic->date = i_pts;
                   p_block = id->p_encoder->pf_encode_video(id->p_encoder, p_pic);
                   block_ChainAppend( out, p_block );
               }
           }
        }

        if( p_sys->i_threads == 0 )
        {
            picture_Release( p_pic );
        }
        else
        {
            vlc_mutex_lock( &p_sys->lock_out );
            p_sys->pp_pics[p_sys->i_last_pic++] = p_pic;
            p_sys->i_last_pic %= PICTURE_RING_SIZE;
            *out = p_sys->p_buffers;
            p_sys->p_buffers = NULL;
            if( p_pic2 != NULL )
            {
                p_sys->pp_pics[p_sys->i_last_pic++] = p_pic2;
                p_sys->i_last_pic %= PICTURE_RING_SIZE;
            }
            vlc_cond_signal( &p_sys->cond );
            vlc_mutex_unlock( &p_sys->lock_out );
        }
    }

    return VLC_SUCCESS;
}

bool transcode_video_add( sout_stream_t *p_stream, es_format_t *p_fmt,
                                sout_stream_id_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    msg_Dbg( p_stream,
             "creating video transcoding from fcc=`%4.4s' to fcc=`%4.4s'",
             (char*)&p_fmt->i_codec, (char*)&p_sys->i_vcodec );

    /* Complete destination format */
    id->p_encoder->fmt_out.i_codec = p_sys->i_vcodec;
    id->p_encoder->fmt_out.video.i_width  = p_sys->i_width & ~1;
    id->p_encoder->fmt_out.video.i_height = p_sys->i_height & ~1;
    id->p_encoder->fmt_out.i_bitrate = p_sys->i_vbitrate;

    /* Build decoder -> filter -> encoder chain */
    if( transcode_video_new( p_stream, id ) )
    {
        msg_Err( p_stream, "cannot create video chain" );
        return false;
    }

    /* Stream will be added later on because we don't know
     * all the characteristics of the decoded stream yet */
    id->b_transcode = true;

    if( p_sys->f_fps > 0 )
    {
        id->p_encoder->fmt_out.video.i_frame_rate = (p_sys->f_fps * ENC_FRAMERATE_BASE) + 0.5;
        id->p_encoder->fmt_out.video.i_frame_rate_base = ENC_FRAMERATE_BASE;
    }

    return true;
}

