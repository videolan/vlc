/*****************************************************************************
 * encoder.c: video and audio encoder using the ffmpeg library
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: encoder.c,v 1.17 2003/11/29 18:36:13 massiot Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/aout.h>
#include <vlc/decoder.h>

/* ffmpeg header */
#define HAVE_MMX
#ifdef HAVE_FFMPEG_AVCODEC_H
#   include <ffmpeg/avcodec.h>
#else
#   include <avcodec.h>
#endif

#include "ffmpeg.h"

#define AVCODEC_MAX_VIDEO_FRAME_SIZE (3*1024*1024)
#define HURRY_UP_GUARD (200000)

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
int  E_(OpenEncoder) ( vlc_object_t * );
void E_(CloseEncoder)( vlc_object_t * );

static block_t *EncodeVideo( encoder_t *, picture_t * );
static block_t *EncodeAudio( encoder_t *, aout_buffer_t * );

/*****************************************************************************
 * encoder_sys_t : ffmpeg encoder descriptor
 *****************************************************************************/
struct encoder_sys_t
{
    /*
     * Ffmpeg properties
     */
    AVCodec         *p_codec;
    AVCodecContext  *p_context;

    /*
     * Common properties
     */
    char *p_buffer;
    char *p_buffer_out;

    /*
     * Videoo properties
     */
    mtime_t i_last_ref_pts;
    mtime_t i_buggy_pts_detect;

    /*
     * Audio properties
     */
    int i_frame_size;
    int i_samples_delay;
    mtime_t i_pts;
};

/*****************************************************************************
 * OpenEncoder: probe the encoder
 *****************************************************************************/
int E_(OpenEncoder)( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys = p_enc->p_sys;
    AVCodecContext *p_context;
    AVCodec *p_codec;
    int i_codec_id, i_cat;
    char *psz_namecodec;

    if( !E_(GetFfmpegCodec)( p_enc->fmt_out.i_codec, &i_cat, &i_codec_id,
                             &psz_namecodec ) )
    {
        if( E_(GetFfmpegChroma)( p_enc->fmt_out.i_codec ) < 0 )
        {
            /* handed chroma output */
            return VLC_EGENERIC;
        }
        i_cat      = VIDEO_ES;
        i_codec_id = CODEC_ID_RAWVIDEO;
        psz_namecodec = "Raw video";
    }


    if( p_enc->fmt_out.i_cat == VIDEO_ES && i_cat != VIDEO_ES )
    {
        msg_Err( p_enc, "\"%s\" is not a video encoder", psz_namecodec );
        return VLC_EGENERIC;
    }

    if( p_enc->fmt_out.i_cat == AUDIO_ES && i_cat != AUDIO_ES )
    {
        msg_Err( p_enc, "\"%s\" is not an audio encoder", psz_namecodec );
        return VLC_EGENERIC;
    }

    /* Initialization must be done before avcodec_find_decoder() */
    E_(InitLibavcodec)(p_this);

    p_codec = avcodec_find_encoder( i_codec_id );
    if( !p_codec )
    {
        msg_Err( p_enc, "cannot find encoder %s", psz_namecodec );
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_sys = (encoder_sys_t *)malloc(sizeof(encoder_sys_t)) ) == NULL )
    {
        msg_Err( p_enc, "out of memory" );
        return VLC_EGENERIC;
    }
    p_enc->p_sys = p_sys;
    p_sys->p_codec = p_codec;

    p_enc->pf_encode_video = EncodeVideo;
    p_enc->pf_encode_audio = EncodeAudio;

    p_sys->p_buffer_out = NULL;
    p_sys->p_buffer = NULL;

    p_sys->p_context = p_context = avcodec_alloc_context();

    /* Set CPU capabilities */
    p_context->dsp_mask = 0;
    if( !(p_enc->p_libvlc->i_cpu & CPU_CAPABILITY_MMX) )
    {
        p_context->dsp_mask |= FF_MM_MMX;
    }
    if( !(p_enc->p_libvlc->i_cpu & CPU_CAPABILITY_MMXEXT) )
    {
        p_context->dsp_mask |= FF_MM_MMXEXT;
    }
    if( !(p_enc->p_libvlc->i_cpu & CPU_CAPABILITY_3DNOW) )
    {
        p_context->dsp_mask |= FF_MM_3DNOW;
    }
    if( !(p_enc->p_libvlc->i_cpu & CPU_CAPABILITY_SSE) )
    {
        p_context->dsp_mask |= FF_MM_SSE;
        p_context->dsp_mask |= FF_MM_SSE2;
    }

    /* Make sure we get extradata filled by the encoder */
    p_context->extradata_size = 0;
    p_context->extradata = NULL;
    p_context->flags |= CODEC_FLAG_GLOBAL_HEADER;

    if( p_enc->fmt_in.i_cat == VIDEO_ES )
    {
        p_context->width = p_enc->fmt_in.video.i_width;
        p_context->height = p_enc->fmt_in.video.i_height;

        p_context->frame_rate = p_enc->fmt_in.video.i_frame_rate;
        p_context->frame_rate_base= p_enc->fmt_in.video.i_frame_rate_base;

#if LIBAVCODEC_BUILD >= 4687
        p_context->sample_aspect_ratio =
            av_d2q( p_enc->fmt_in.video.i_aspect * p_context->height /
                    p_context->width / VOUT_ASPECT_FACTOR, 255 );
#else
        p_context->aspect_ratio = ((float)p_enc->fmt_in.video.i_aspect) /
            VOUT_ASPECT_FACTOR;
#endif

        p_sys->p_buffer_out = malloc( AVCODEC_MAX_VIDEO_FRAME_SIZE );

        p_enc->fmt_in.i_codec = VLC_FOURCC('I','4','2','0');

        if ( p_enc->b_strict_rc )
        {
            p_context->rc_max_rate = p_enc->fmt_out.i_bitrate;
            p_context->rc_buffer_size = p_context->bit_rate / 2;
            p_context->rc_buffer_aggressivity = 1000.0; /* FIXME */
        }

        if ( p_enc->b_pre_me )
        {
            p_context->pre_me = 1;
            p_context->me_pre_cmp = FF_CMP_CHROMA;
        }
    }
    else if( p_enc->fmt_in.i_cat == AUDIO_ES )
    {
        p_enc->fmt_in.i_codec  = AOUT_FMT_S16_NE;
        p_context->sample_rate = p_enc->fmt_in.audio.i_rate;
        p_context->channels    = p_enc->fmt_in.audio.i_channels;
        p_sys->i_frame_size = p_context->frame_size * 2 * p_context->channels;
        p_sys->p_buffer = malloc( p_sys->i_frame_size );
        p_sys->p_buffer_out = malloc( 2 * AVCODEC_MAX_AUDIO_FRAME_SIZE );
    }

    /* Misc parameters */
    p_context->bit_rate = p_enc->fmt_out.i_bitrate;
    p_context->gop_size = p_enc->i_key_int > 0 ? p_enc->i_key_int : 50;
    p_context->max_b_frames =
        __MIN( p_enc->i_b_frames, FF_MAX_B_FRAMES );
    p_context->b_frame_strategy = 0;
    p_context->b_quant_factor = 2.0;

    if( p_enc->i_vtolerance > 0 )
    {
        p_context->bit_rate_tolerance = p_enc->i_vtolerance;
    }
    p_context->qmin = p_enc->i_qmin;
    p_context->qmax = p_enc->i_qmax;

    p_context->mb_decision = p_enc->i_hq;

    if( i_codec_id == CODEC_ID_RAWVIDEO )
    {
        /* XXX: hack: Force same codec (will be handled by transcode) */
        p_enc->fmt_in.i_codec = p_enc->fmt_out.i_codec;
        p_context->pix_fmt = E_(GetFfmpegChroma)( p_enc->fmt_in.i_codec );
    }

    /* Make sure we get extradata filled by the encoder */
    p_context->extradata_size = 0;
    p_context->extradata = NULL;
    p_context->flags |= CODEC_FLAG_GLOBAL_HEADER;

    if( avcodec_open( p_context, p_codec ) )
    {
        if( p_enc->fmt_in.i_cat == AUDIO_ES && p_context->channels > 2 )
        {
            p_context->channels = 2;
            p_enc->fmt_in.audio.i_channels = 2; // FIXME
            if( avcodec_open( p_context, p_codec ) )
            {
                msg_Err( p_enc, "cannot open encoder" );
                return VLC_EGENERIC;
            }
            msg_Warn( p_enc, "stereo mode selected (codec limitation)" );
        }
        else
        {
            msg_Err( p_enc, "cannot open encoder" );
            return VLC_EGENERIC;
        }
    }

    p_enc->fmt_out.i_extra = p_context->extradata_size;
    p_enc->fmt_out.p_extra = p_context->extradata;
    p_context->flags &= ~CODEC_FLAG_GLOBAL_HEADER;

    if( p_enc->fmt_in.i_cat == AUDIO_ES )
    {
        p_sys->i_frame_size = p_context->frame_size * 2 * p_context->channels;
        p_sys->p_buffer = malloc( p_sys->i_frame_size );
    }

    p_sys->i_last_ref_pts = 0;
    p_sys->i_buggy_pts_detect = 0;
    p_sys->i_samples_delay = 0;
    p_sys->i_pts = 0;

    msg_Dbg( p_enc, "found encoder %s", psz_namecodec );

    return VLC_SUCCESS;
}

/****************************************************************************
 * EncodeVideo: the whole thing
 ****************************************************************************/
static block_t *EncodeVideo( encoder_t *p_enc, picture_t *p_pict )
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    AVFrame frame;
    int i_out, i_plane;
    vlc_bool_t b_hurry_up = 0;

    memset( &frame, 0, sizeof( AVFrame ) );
    for( i_plane = 0; i_plane < p_pict->i_planes; i_plane++ )
    {
        frame.data[i_plane] = p_pict->p[i_plane].p_pixels;
        frame.linesize[i_plane] = p_pict->p[i_plane].i_pitch;
    }

    /* Set the pts of the frame being encoded (segfaults with mpeg4!)*/
    if( p_enc->fmt_out.i_codec == VLC_FOURCC( 'm', 'p', 'g', 'v' ) ||
        p_enc->fmt_out.i_codec == VLC_FOURCC( 'm', 'p', '1', 'v' ) ||
        p_enc->fmt_out.i_codec == VLC_FOURCC( 'm', 'p', '2', 'v' ) )
    {
        frame.pts = p_pict->date;
        if ( frame.pts && mdate() + HURRY_UP_GUARD > frame.pts
              && p_enc->b_hurry_up )
        {
            msg_Dbg( p_enc, "hurry up mode" );
            p_sys->p_context->mb_decision = FF_MB_DECISION_SIMPLE;
            b_hurry_up = 1;
        }
    }
    else
    {
        frame.pts = 0;
    }

    /* Let ffmpeg select the frame type */
    frame.pict_type = 0;
    frame.repeat_pict = p_pict->i_nb_fields;

#if LIBAVCODEC_BUILD >= 4684
    frame.interlaced_frame = !p_pict->b_progressive;
    frame.top_field_first = p_pict->b_top_field_first;
#endif

    i_out = avcodec_encode_video( p_sys->p_context, p_sys->p_buffer_out,
                                  AVCODEC_MAX_VIDEO_FRAME_SIZE, &frame );

    if ( b_hurry_up )
    {
        p_sys->p_context->mb_decision = p_enc->i_hq;
    }

    if( i_out > 0 )
    {
        block_t *p_block = block_New( p_enc, i_out );
        memcpy( p_block->p_buffer, p_sys->p_buffer_out, i_out );

        if( p_sys->p_context->coded_frame->pts != 0 &&
            p_sys->i_buggy_pts_detect != p_sys->p_context->coded_frame->pts )
        {
            p_sys->i_buggy_pts_detect = p_sys->p_context->coded_frame->pts;

            /* FIXME, 3-2 pulldown is not handled correctly */
            p_block->i_length = I64C(1000000) *
                p_enc->fmt_in.video.i_frame_rate_base /
                p_enc->fmt_in.video.i_frame_rate;
            p_block->i_pts    = p_sys->p_context->coded_frame->pts;

            if( !p_sys->p_context->delay ||
                ( p_sys->p_context->coded_frame->pict_type != FF_I_TYPE &&
                  p_sys->p_context->coded_frame->pict_type != FF_P_TYPE ) )
            {
                p_block->i_dts = p_block->i_pts;
            }
            else
            {
                if( p_sys->i_last_ref_pts )
                {
                    p_block->i_dts = p_sys->i_last_ref_pts;
                }
                else
                {
                    /* Let's put something sensible */
                    p_block->i_dts = p_block->i_pts;
                }

                p_sys->i_last_ref_pts = p_block->i_pts;
            }
        }
        else
        {
            /* Buggy libavcodec which doesn't update coded_frame->pts
             * correctly */
            p_block->i_length = I64C(1000000) *
                p_enc->fmt_in.video.i_frame_rate_base /
                p_enc->fmt_in.video.i_frame_rate;
            p_block->i_dts = p_block->i_pts = p_pict->date;
        }

        return p_block;
    }

    return NULL;
}

/****************************************************************************
 * EncodeAudio: the whole thing
 ****************************************************************************/
static block_t *EncodeAudio( encoder_t *p_enc, aout_buffer_t *p_aout_buf )
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    block_t *p_block, *p_chain = NULL;

    char *p_buffer = p_aout_buf->p_buffer;
    int i_samples = p_aout_buf->i_nb_samples;
    int i_samples_delay = p_sys->i_samples_delay;

    p_sys->i_pts = p_aout_buf->start_date -
                (mtime_t)1000000 * (mtime_t)p_sys->i_samples_delay /
                (mtime_t)p_enc->fmt_in.audio.i_rate;

    p_sys->i_samples_delay += i_samples;

    while( p_sys->i_samples_delay >= p_sys->p_context->frame_size )
    {
        int16_t *p_samples;
        int i_out;

        if( i_samples_delay )
        {
            /* Take care of the left-over from last time */
            int i_delay_size = i_samples_delay * 2 *
                                 p_sys->p_context->channels;
            int i_size = p_sys->i_frame_size - i_delay_size;

            p_samples = (int16_t *)p_sys->p_buffer;
            memcpy( p_sys->p_buffer + i_delay_size, p_buffer, i_size );
            p_buffer -= i_delay_size;
            i_samples += i_samples_delay;
            i_samples_delay = 0;
        }
        else
        {
            p_samples = (int16_t *)p_buffer;
        }

        i_out = avcodec_encode_audio( p_sys->p_context, p_sys->p_buffer_out,
                                      2 * AVCODEC_MAX_AUDIO_FRAME_SIZE,
                                      p_samples );

#if 0
        msg_Warn( p_enc, "avcodec_encode_audio: %d", i_out );
#endif
        if( i_out < 0 ) break;

        p_buffer += p_sys->i_frame_size;
        p_sys->i_samples_delay -= p_sys->p_context->frame_size;
        i_samples -= p_sys->p_context->frame_size;

        if( i_out == 0 ) continue;

        p_block = block_New( p_enc, i_out );
        memcpy( p_block->p_buffer, p_sys->p_buffer_out, i_out );

        p_block->i_length = (mtime_t)1000000 *
            (mtime_t)p_sys->p_context->frame_size /
            (mtime_t)p_sys->p_context->sample_rate;

        p_block->i_dts = p_block->i_pts = p_sys->i_pts;

        /* Update pts */
        p_sys->i_pts += p_block->i_length;
        block_ChainAppend( &p_chain, p_block );
    }

    /* Backup the remaining raw samples */
    if( i_samples )
    {
        memcpy( p_sys->p_buffer, p_buffer + i_samples_delay * 2 *
                p_sys->p_context->channels,
                i_samples * 2 * p_sys->p_context->channels );
    }

    return p_chain;
}

/*****************************************************************************
 * CloseEncoder: ffmpeg encoder destruction
 *****************************************************************************/
void E_(CloseEncoder)( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys = p_enc->p_sys;

    avcodec_close( p_sys->p_context );
    free( p_sys->p_context );

    if( p_sys->p_buffer ) free( p_sys->p_buffer );
    if( p_sys->p_buffer_out ) free( p_sys->p_buffer_out );

    free( p_sys );
}
