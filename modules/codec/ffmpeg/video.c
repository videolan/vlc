/*****************************************************************************
 * video.c: video decoder using the ffmpeg library
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: video.c,v 1.43 2003/10/28 14:17:52 gbazin Exp $
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/decoder.h>
#include <vlc/input.h>

#ifdef HAVE_SYS_TIMES_H
#   include <sys/times.h>
#endif

/* ffmpeg header */
#ifdef HAVE_FFMPEG_AVCODEC_H
#   include <ffmpeg/avcodec.h>
#else
#   include <avcodec.h>
#endif

#include "ffmpeg.h"

/*****************************************************************************
 * decoder_sys_t : decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    /* Common part between video and audio decoder */
    int i_cat;
    int i_codec_id;
    char *psz_namecodec;

    AVCodecContext      *p_context;
    AVCodec             *p_codec;

    /* Video decoder specific part */
    mtime_t input_pts;
    mtime_t i_pts;

    AVFrame          *p_ff_pic;
    BITMAPINFOHEADER *p_format;

    vout_thread_t    *p_vout;

    /* for frame skipping algo */
    int b_hurry_up;
    int i_frame_error;
    int i_frame_skip;

    /* how many decoded frames are late */
    int     i_late_frames;
    mtime_t i_late_frames_start;

    /* for direct rendering */
    int b_direct_rendering;

    vlc_bool_t b_has_b_frames;

    int i_buffer;
    char *p_buffer;

    /* Postprocessing handle */
    void *p_pp;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void ffmpeg_CopyPicture    ( decoder_t *, picture_t *, AVFrame * );
static int  ffmpeg_GetFrameBuf    ( struct AVCodecContext *, AVFrame * );
static void ffmpeg_ReleaseFrameBuf( struct AVCodecContext *, AVFrame * );

/*****************************************************************************
 * Local Functions
 *****************************************************************************/
static inline uint32_t ffmpeg_PixFmtToChroma( int i_ff_chroma )
{
    /* FIXME FIXME some of them are wrong */
    switch( i_ff_chroma )
    {
    case PIX_FMT_YUV420P:
    case PIX_FMT_YUV422:
        return( VLC_FOURCC('I','4','2','0') );
    case PIX_FMT_RGB24:
        return( VLC_FOURCC('R','V','2','4') );
    case PIX_FMT_YUV422P:
        return( VLC_FOURCC('I','4','2','2') );
    case PIX_FMT_YUV444P:
        return( VLC_FOURCC('I','4','4','4') );
    case PIX_FMT_YUV410P:
    case PIX_FMT_YUV411P:
    case PIX_FMT_BGR24:
    default:
        return 0;
    }
}

/* Return a Vout */
static vout_thread_t *ffmpeg_CreateVout( decoder_t  *p_dec,
                                         AVCodecContext *p_context )
{
    vout_thread_t *p_vout;
    unsigned int   i_width = p_context->width;
    unsigned int   i_height = p_context->height;
    uint32_t       i_chroma = ffmpeg_PixFmtToChroma( p_context->pix_fmt );
    unsigned int   i_aspect;

    if( !i_width || !i_height )
    {
        return( NULL ); /* Can't create a new vout without display size */
    }

    if( !i_chroma )
    {
        /* we make conversion if possible*/
        i_chroma = VLC_FOURCC('I','4','2','0');
    }

#if LIBAVCODEC_BUILD >= 4687
    i_aspect = VOUT_ASPECT_FACTOR * ( av_q2d(p_context->sample_aspect_ratio) *
        p_context->width / p_context->height );
#else
    i_aspect = VOUT_ASPECT_FACTOR * p_context->aspect_ratio;
#endif
    if( i_aspect == 0 )
    {
        i_aspect = VOUT_ASPECT_FACTOR * i_width / i_height;
    }

    /* Spawn a video output if there is none. First we look for our children,
     * then we look for any other vout that might be available. */
    p_vout = vout_Request( p_dec, p_dec->p_sys->p_vout,
                           i_width, i_height, i_chroma, i_aspect );

#ifdef LIBAVCODEC_PP
    if( p_dec->p_sys->p_pp )
        E_(InitPostproc)( p_dec, p_dec->p_sys->p_pp, i_width, i_height,
                          p_context->pix_fmt );
#endif

    return p_vout;
}

/*****************************************************************************
 * InitVideo: initialize the video decoder
 *****************************************************************************
 * the ffmpeg codec will be opened, some memory allocated. The vout is not yet
 * opened (done after the first decoded frame).
 *****************************************************************************/
int E_(InitVideoDec)( decoder_t *p_dec, AVCodecContext *p_context,
                      AVCodec *p_codec, int i_codec_id, char *psz_namecodec )
{
    decoder_sys_t *p_sys;
    vlc_value_t val;
    int i_tmp;

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys =
          (decoder_sys_t *)malloc(sizeof(decoder_sys_t)) ) == NULL )
    {
        msg_Err( p_dec, "out of memory" );
        return VLC_EGENERIC;
    }

    p_dec->p_sys->p_context = p_context;
    p_dec->p_sys->p_codec = p_codec;
    p_dec->p_sys->i_codec_id = i_codec_id;
    p_dec->p_sys->psz_namecodec = psz_namecodec;
    p_sys->p_ff_pic = avcodec_alloc_frame();

    if( ( p_sys->p_format =
          (BITMAPINFOHEADER *)p_dec->p_fifo->p_bitmapinfoheader ) != NULL )
    {
        /* ***** Fill p_context with init values ***** */
        p_sys->p_context->width  = p_sys->p_format->biWidth;
        p_sys->p_context->height = p_sys->p_format->biHeight;
    }
    else
    {
        msg_Warn( p_dec, "display informations missing" );
        p_sys->p_format = NULL;
    }

    /*  ***** Get configuration of ffmpeg plugin ***** */
    i_tmp = config_GetInt( p_dec, "ffmpeg-workaround-bugs" );
    p_sys->p_context->workaround_bugs  = __MAX( __MIN( i_tmp, 99 ), 0 );

    i_tmp = config_GetInt( p_dec, "ffmpeg-error-resilience" );
    p_sys->p_context->error_resilience = __MAX( __MIN( i_tmp, 99 ), -1 );

    var_Create( p_dec, "grayscale", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Get( p_dec, "grayscale", &val );
    if( val.b_bool ) p_sys->p_context->flags |= CODEC_FLAG_GRAY;

    /* Decide if we set CODEC_FLAG_TRUNCATED */
#if LIBAVCODEC_BUILD >= 4662
    var_Create( p_dec, "ffmpeg-truncated", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );
    var_Get( p_dec, "ffmpeg-truncated", &val );
    if( val.i_int > 0 ) p_sys->p_context->flags |= CODEC_FLAG_TRUNCATED;
#endif

    /* ***** Open the codec ***** */
    if( avcodec_open( p_sys->p_context, p_sys->p_codec ) < 0 )
    {
        msg_Err( p_dec, "cannot open codec (%s)", p_sys->psz_namecodec );
        return VLC_EGENERIC;
    }
    else
    {
        msg_Dbg( p_dec, "ffmpeg codec (%s) started", p_sys->psz_namecodec );
    }

    /* ***** ffmpeg frame skipping ***** */
    var_Create( p_dec, "ffmpeg-hurry-up", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Get( p_dec, "ffmpeg-hurry-up", &val );
    p_sys->b_hurry_up = val.b_bool;

    /* ***** ffmpeg direct rendering ***** */
    p_sys->b_direct_rendering = 0;
    var_Create( p_dec, "ffmpeg-dr", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Get( p_dec, "ffmpeg-dr", &val );
    if( val.b_bool && (p_sys->p_codec->capabilities & CODEC_CAP_DR1) &&
        ffmpeg_PixFmtToChroma( p_sys->p_context->pix_fmt ) &&
        /* Apparently direct rendering doesn't work with YUV422P */
        p_sys->p_context->pix_fmt != PIX_FMT_YUV422P &&
        !(p_sys->p_context->width % 16) && !(p_sys->p_context->height % 16) )
    {
        /* Some codecs set pix_fmt only after the 1st frame has been decoded,
         * so we need to do another check in ffmpeg_GetFrameBuf() */
        p_sys->b_direct_rendering = 1;
    }

#ifdef LIBAVCODEC_PP
    p_sys->p_pp = NULL;
    if( E_(OpenPostproc)( p_dec, &p_sys->p_pp ) == VLC_SUCCESS )
    {
        /* for now we cannot do postproc and dr */
        p_sys->b_direct_rendering = 0;
    }
#endif

    /* ffmpeg doesn't properly release old pictures when frames are skipped */
    if( p_sys->b_hurry_up ) p_sys->b_direct_rendering = 0;
    if( p_sys->b_direct_rendering )
    {
        msg_Dbg( p_dec, "using direct rendering" );
        p_sys->p_context->flags |= CODEC_FLAG_EMU_EDGE;
    }

    /* Always use our get_buffer wrapper so we can calculate the
     * PTS correctly */
    p_sys->p_context->get_buffer = ffmpeg_GetFrameBuf;
    p_sys->p_context->release_buffer = ffmpeg_ReleaseFrameBuf;
    p_sys->p_context->opaque = p_dec;

    /* ***** init this codec with special data ***** */
    if( p_sys->p_format && p_sys->p_format->biSize > sizeof(BITMAPINFOHEADER) )
    {
        int b_gotpicture;
        int i_size = p_sys->p_format->biSize - sizeof(BITMAPINFOHEADER);

        if( p_sys->i_codec_id == CODEC_ID_MPEG4 )
        {
            uint8_t *p_vol = malloc( i_size + FF_INPUT_BUFFER_PADDING_SIZE );

            memcpy( p_vol, &p_sys->p_format[1], i_size );
            memset( &p_vol[i_size], 0, FF_INPUT_BUFFER_PADDING_SIZE );

            avcodec_decode_video( p_sys->p_context, p_sys->p_ff_pic,
                                  &b_gotpicture, p_vol, i_size );
            free( p_vol );
        }
#if LIBAVCODEC_BUILD >= 4666
        else if( p_sys->i_codec_id == CODEC_ID_SVQ3 )
        {
            uint8_t *p;

            p_sys->p_context->extradata_size = i_size + 12;
            p = p_sys->p_context->extradata  =
                malloc( p_sys->p_context->extradata_size );

            memcpy( &p[0],  "SVQ3", 4 );
            memset( &p[4], 0, 8 );
            memcpy( &p[12], &p_sys->p_format[1], i_size );
        }
#endif
        else
        {
            p_sys->p_context->extradata_size = i_size;
            p_sys->p_context->extradata =
                malloc( i_size + FF_INPUT_BUFFER_PADDING_SIZE );
            memcpy( p_sys->p_context->extradata,
                    &p_sys->p_format[1], i_size );
            memset( &((uint8_t*)p_sys->p_context->extradata)[i_size],
                    0, FF_INPUT_BUFFER_PADDING_SIZE );
        }
    }

    /* ***** misc init ***** */
    p_sys->p_vout = NULL;
    p_sys->input_pts = 0;
    p_sys->i_pts = 0;
    p_sys->b_has_b_frames = VLC_FALSE;
    p_sys->i_late_frames = 0;
    p_sys->i_buffer = 1;
    p_sys->p_buffer = malloc( p_sys->i_buffer );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DecodeVideo: Called to decode one or more frames
 *****************************************************************************/
int E_(DecodeVideo)( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int i_buffer, b_drawpicture;
    char *p_buffer;

    if( p_block->i_pts > 0 )
    {
        p_sys->input_pts = p_block->i_pts;
    }

    /* TODO implement it in a better way */
    /* A good idea could be to decode all I pictures and see for the other */
    if( p_sys->b_hurry_up && p_sys->i_late_frames > 4 )
    {
        b_drawpicture = 0;
        if( p_sys->i_late_frames < 8 )
        {
            p_sys->p_context->hurry_up = 2;
        }
        else
        {
            /* picture too late, won't decode
             * but break picture until a new I, and for mpeg4 ...*/

            p_sys->i_late_frames--; /* needed else it will never be decrease */
            block_Release( p_block );
            return VLC_SUCCESS;
        }
    }
    else
    {
        b_drawpicture = 1;
        p_sys->p_context->hurry_up = 0;
    }

    if( p_sys->i_late_frames > 0 &&
        mdate() - p_sys->i_late_frames_start > I64C(5000000) )
    {
        msg_Err( p_dec, "more than 5 seconds of late video -> "
                 "dropping frame (computer too slow ?)" );
        block_Release( p_block );
        p_sys->i_pts = 0; /* To make sure we recover properly */
        p_sys->i_late_frames--;
        return VLC_SUCCESS;
    }

    if( !p_sys->p_context->width || !p_sys->p_context->height )
    {
        p_sys->p_context->hurry_up = 5;
    }

    /*
     * Do the actual decoding now
     */

    /* Don't forget that ffmpeg requires a little more bytes
     * that the real frame size */
    i_buffer = p_block->i_buffer;
    if( i_buffer + FF_INPUT_BUFFER_PADDING_SIZE > p_sys->i_buffer )
    {
        free( p_sys->p_buffer );
        p_sys->i_buffer = i_buffer + FF_INPUT_BUFFER_PADDING_SIZE;
        p_sys->p_buffer = malloc( p_sys->i_buffer );
    }
    p_buffer = p_sys->p_buffer;
    p_dec->p_vlc->pf_memcpy( p_buffer, p_block->p_buffer, p_block->i_buffer );
    memset( p_buffer + i_buffer, 0, FF_INPUT_BUFFER_PADDING_SIZE );

    while( i_buffer )
    {
        int i_used, b_gotpicture;
        picture_t *p_pic;

        i_used = avcodec_decode_video( p_sys->p_context, p_sys->p_ff_pic,
                                       &b_gotpicture,
                                       p_buffer, i_buffer );
        if( i_used < 0 )
        {
            msg_Warn( p_dec, "cannot decode one frame (%d bytes)", i_buffer );
            p_sys->i_frame_error++;
            block_Release( p_block );
            return VLC_SUCCESS;
        }

        /* Consumed bytes */
        i_buffer -= i_used;
        p_buffer += i_used;

        /* Nothing to display */
        if( !b_gotpicture ) continue;

        /* Update frame late count*/
        if( p_sys->i_pts && p_sys->i_pts <= mdate() )
        {
            p_sys->i_late_frames++;
            if( p_sys->i_late_frames == 1 )
                p_sys->i_late_frames_start = mdate();
        }
        else
        {
            p_sys->i_late_frames = 0;
        }

        if( !b_drawpicture || p_sys->p_ff_pic->linesize[0] == 0 )
        {
            /* Do not display the picture */
            continue;
        }

        if( !p_sys->b_direct_rendering )
        {
            p_sys->p_vout = ffmpeg_CreateVout( p_dec, p_sys->p_context );
            if( !p_sys->p_vout )
            {
                msg_Err( p_dec, "cannot create vout" );
                block_Release( p_block );
                return VLC_EGENERIC;
            }

            /* Get a new picture */
            while( !(p_pic = vout_CreatePicture( p_sys->p_vout, 0, 0, 0 ) ) )
            {
                if( p_dec->p_fifo->b_die || p_dec->p_fifo->b_error )
                {
                    block_Release( p_block );
                    return VLC_EGENERIC;
                }
                msleep( VOUT_OUTMEM_SLEEP );
            }

            /* Fill p_picture_t from AVVideoFrame and do chroma conversion
             * if needed */
            ffmpeg_CopyPicture( p_dec, p_pic, p_sys->p_ff_pic );
        }
        else
        {
            p_pic = (picture_t *)p_sys->p_ff_pic->opaque;
        }

        /* Set the PTS
         * There is an ugly hack here because some demuxers pass us a dts
         * instead of a pts so this screw up things for streams with
         * B frames. */
        if( p_sys->p_ff_pic->pict_type == FF_B_TYPE )
            p_sys->b_has_b_frames = VLC_TRUE;
        if( p_sys->p_ff_pic->pts &&
            ( !p_sys->p_context->has_b_frames || !p_sys->b_has_b_frames ||
              p_sys->p_ff_pic->pict_type == FF_B_TYPE ) )
        {
            p_sys->i_pts = p_sys->p_ff_pic->pts;
        }

        /* Send decoded frame to vout */
        if( p_sys->i_pts )
        {
            vout_DatePicture( p_sys->p_vout, p_pic, p_sys->i_pts );
            vout_DisplayPicture( p_sys->p_vout, p_pic );

            /* interpolate the next PTS */
            if( p_sys->p_context->frame_rate > 0 )
            {
                p_sys->i_pts += I64C(1000000) *
                    (2 + p_sys->p_ff_pic->repeat_pict) *
                    p_sys->p_context->frame_rate_base /
                    (2 * p_sys->p_context->frame_rate);
            }
        }
    }

    block_Release( p_block );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * EndVideo: decoder destruction
 *****************************************************************************
 * This function is called when the thread ends after a sucessful
 * initialization.
 *****************************************************************************/
void E_(EndVideoDec)( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->p_ff_pic ) free( p_sys->p_ff_pic );

#ifdef LIBAVCODEC_PP
    E_(ClosePostproc)( p_dec, p_sys->p_pp );
#endif

    free( p_sys->p_buffer );

    /* We are about to die. Reattach video output to p_vlc. */
    vout_Request( p_dec, p_sys->p_vout, 0, 0, 0, 0 );
}

/*****************************************************************************
 * ffmpeg_CopyPicture: copy a picture from ffmpeg internal buffers to a
 *                     picture_t structure (when not in direct rendering mode).
 *****************************************************************************/
static void ffmpeg_CopyPicture( decoder_t *p_dec,
                                picture_t *p_pic, AVFrame *p_ff_pic )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( ffmpeg_PixFmtToChroma( p_sys->p_context->pix_fmt ) )
    {
        int i_plane, i_size, i_line;
        uint8_t *p_dst, *p_src;
        int i_src_stride, i_dst_stride;

#ifdef LIBAVCODEC_PP
        if( p_sys->p_pp )
            E_(PostprocPict)( p_dec, p_sys->p_pp, p_pic, p_ff_pic );
        else
#endif
        for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
        {
            p_src  = p_ff_pic->data[i_plane];
            p_dst = p_pic->p[i_plane].p_pixels;
            i_src_stride = p_ff_pic->linesize[i_plane];
            i_dst_stride = p_pic->p[i_plane].i_pitch;

            i_size = __MIN( i_src_stride, i_dst_stride );
            for( i_line = 0; i_line < p_pic->p[i_plane].i_lines; i_line++ )
            {
                p_dec->p_vlc->pf_memcpy( p_dst, p_src, i_size );
                p_src += i_src_stride;
                p_dst += i_dst_stride;
            }
        }
    }
    else
    {
        AVPicture dest_pic;
        int i;

        /* we need to convert to I420 */
        switch( p_sys->p_context->pix_fmt )
        {
        case( PIX_FMT_YUV410P ):
        case( PIX_FMT_YUV411P ):
            for( i = 0; i < p_pic->i_planes; i++ )
            {
                dest_pic.data[i] = p_pic->p[i].p_pixels;
                dest_pic.linesize[i] = p_pic->p[i].i_pitch;
            }
            img_convert( &dest_pic, PIX_FMT_YUV420P,
                         (AVPicture *)p_ff_pic,
                         p_sys->p_context->pix_fmt,
                         p_sys->p_context->width,
                         p_sys->p_context->height );
            break;
        default:
            msg_Err( p_dec, "don't know how to convert chroma %i",
                     p_sys->p_context->pix_fmt );
            p_dec->p_fifo->b_error = 1;
            break;
        }
    }
}

/*****************************************************************************
 * ffmpeg_GetFrameBuf: callback used by ffmpeg to get a frame buffer.
 *****************************************************************************
 * It is used for direct rendering as well as to get the right PTS for each
 * decoded picture (even in indirect rendering mode).
 *****************************************************************************/
static int ffmpeg_GetFrameBuf( struct AVCodecContext *p_context,
                               AVFrame *p_ff_pic )
{
    decoder_t *p_dec = (decoder_t *)p_context->opaque;
    decoder_sys_t *p_sys = p_dec->p_sys;
    picture_t *p_pic;

    /* Set picture PTS */
    p_ff_pic->pts = p_sys->input_pts;
    p_sys->input_pts = 0;

    /* Not much to do in indirect rendering mode */
    if( !p_sys->b_direct_rendering )
    {
        return avcodec_default_get_buffer( p_context, p_ff_pic );
    }

    /* Some codecs set pix_fmt only after the 1st frame has been decoded,
     * so this check is necessary. */
    if( !ffmpeg_PixFmtToChroma( p_context->pix_fmt ) ||
        p_sys->p_context->width % 16 || p_sys->p_context->height % 16 )
    {
        msg_Dbg( p_dec, "disabling direct rendering" );
        p_sys->b_direct_rendering = 0;
        return avcodec_default_get_buffer( p_context, p_ff_pic );
    }

    /* Check and (re)create our vout if needed */
    p_sys->p_vout = ffmpeg_CreateVout( p_dec, p_sys->p_context );
    if( !p_sys->p_vout )
    {
        msg_Err( p_dec, "cannot create vout" );
        p_dec->p_fifo->b_error = 1; /* abort */
        p_sys->b_direct_rendering = 0;
        return avcodec_default_get_buffer( p_context, p_ff_pic );
    }

    p_sys->p_vout->render.b_allow_modify_pics = 0;

    /* Get a new picture */
    while( !(p_pic = vout_CreatePicture( p_sys->p_vout, 0, 0, 0 ) ) )
    {
        if( p_dec->p_fifo->b_die || p_dec->p_fifo->b_error )
        {
            p_sys->b_direct_rendering = 0;
            return avcodec_default_get_buffer( p_context, p_ff_pic );
        }
        msleep( VOUT_OUTMEM_SLEEP );
    }
    p_sys->p_context->draw_horiz_band = NULL;

    p_ff_pic->opaque = (void*)p_pic;
    p_ff_pic->type = FF_BUFFER_TYPE_USER;
    p_ff_pic->data[0] = p_pic->p[0].p_pixels;
    p_ff_pic->data[1] = p_pic->p[1].p_pixels;
    p_ff_pic->data[2] = p_pic->p[2].p_pixels;
    p_ff_pic->data[3] = NULL; /* alpha channel but I'm not sure */

    p_ff_pic->linesize[0] = p_pic->p[0].i_pitch;
    p_ff_pic->linesize[1] = p_pic->p[1].i_pitch;
    p_ff_pic->linesize[2] = p_pic->p[2].i_pitch;
    p_ff_pic->linesize[3] = 0;

    if( p_ff_pic->reference != 0 )
    {
        vout_LinkPicture( p_sys->p_vout, p_pic );
    }

    /* FIXME what is that, should give good value */
    p_ff_pic->age = 256*256*256*64; // FIXME FIXME from ffmpeg

    return( 0 );
}

static void  ffmpeg_ReleaseFrameBuf( struct AVCodecContext *p_context,
                                     AVFrame *p_ff_pic )
{
    decoder_t *p_dec = (decoder_t *)p_context->opaque;
    decoder_sys_t *p_sys = p_dec->p_sys;
    picture_t *p_pic;

    if( p_ff_pic->type != FF_BUFFER_TYPE_USER )
    {
        avcodec_default_release_buffer( p_context, p_ff_pic );
        return;
    }

    p_pic = (picture_t*)p_ff_pic->opaque;

    p_ff_pic->data[0] = NULL;
    p_ff_pic->data[1] = NULL;
    p_ff_pic->data[2] = NULL;
    p_ff_pic->data[3] = NULL;

    if( p_ff_pic->reference != 0 )
    {
        vout_UnlinkPicture( p_sys->p_vout, p_pic );
    }
}
