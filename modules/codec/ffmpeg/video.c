/*****************************************************************************
 * video.c: video decoder using ffmpeg library
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: video.c,v 1.11 2002/12/06 16:34:05 sam Exp $
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

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/aout.h>
#include <vlc/decoder.h>
#include <vlc/input.h>

#include <string.h>

#ifdef HAVE_SYS_TIMES_H
#   include <sys/times.h>
#endif

#include "avcodec.h"                                               /* ffmpeg */

#include "postprocessing/postprocessing.h"

#include "ffmpeg.h"
#include "video.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#if LIBAVCODEC_BUILD >= 4641
static void ffmpeg_CopyPicture( picture_t *, AVVideoFrame *, vdec_thread_t * );
#else
static void ffmpeg_CopyPicture( picture_t *, AVPicture *, vdec_thread_t * );
#endif

static void ffmpeg_PostProcPicture( vdec_thread_t *, picture_t * );

#if LIBAVCODEC_BUILD >= 4641
static int  ffmpeg_GetFrameBuf( struct AVCodecContext *, AVVideoFrame *);
static void ffmpeg_ReleaseFrameBuf( struct AVCodecContext *, AVVideoFrame *);
#endif

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
#if LIBAVCODEC_BUILD >= 4615
        case PIX_FMT_YUV410P:
#endif
        case PIX_FMT_BGR24:
        default:
            return( 0 );
    }
}

/* Return a Vout */
static vout_thread_t *ffmpeg_CreateVout( vdec_thread_t  *p_vdec,
                                         AVCodecContext *p_context )
{
    vout_thread_t *p_vout;
    unsigned int   i_width = p_context->width;
    unsigned int   i_height =p_context->height;
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
        msg_Warn( p_vdec->p_fifo, "Internal chroma conversion (FIXME)");
        /* It's mainly for I410 -> I420 conversion that I've made,
           it's buggy and very slow */
    }
#if LIBAVCODEC_BUILD >= 4640
    i_aspect = VOUT_ASPECT_FACTOR * p_context->aspect_ratio;
    if( i_aspect == 0 )
    {
        i_aspect = VOUT_ASPECT_FACTOR * i_width / i_height;
    }
#else
    switch( p_context->aspect_ratio_info )
    {
        case( FF_ASPECT_4_3_625 ):
        case( FF_ASPECT_4_3_525 ):
            i_aspect = VOUT_ASPECT_FACTOR * 4 / 3;
            break;
        case( FF_ASPECT_16_9_625 ):
        case( FF_ASPECT_16_9_525 ):
            i_aspect = VOUT_ASPECT_FACTOR * 16 / 9 ;
            break;
        case( FF_ASPECT_SQUARE ):
        default:
            i_aspect = VOUT_ASPECT_FACTOR * i_width / i_height;
            break;
        }
#endif

    /* Spawn a video output if there is none. First we look for our children,
     * then we look for any other vout that might be available. */
    p_vout = vout_Request( p_vdec->p_fifo, NULL,
                           i_width, i_height, i_chroma, i_aspect );

    return p_vout;
}

/* FIXME FIXME FIXME this is a big shit
   does someone want to rewrite this function ?
   or said to me how write a better thing
   FIXME FIXME FIXME
*/
#if LIBAVCODEC_BUILD >= 4641
static void ffmpeg_ConvertPictureI410toI420( picture_t *p_pic,
                                             AVVideoFrame *p_ff_pic,
                                             vdec_thread_t   *p_vdec )
#else
static void ffmpeg_ConvertPictureI410toI420( picture_t *p_pic,
                                             AVPicture *p_ff_pic,
                                             vdec_thread_t   *p_vdec )
#endif
{
    uint8_t *p_src, *p_dst;
    uint8_t *p_plane[3];
    int i_plane;

    int i_stride, i_lines;
    int i_height, i_width;
    int i_y, i_x;

    i_height = p_vdec->p_context->height;
    i_width  = p_vdec->p_context->width;

    p_dst = p_pic->p[0].p_pixels;
    p_src  = p_ff_pic->data[0];

    /* copy first plane */
    for( i_y = 0; i_y < i_height; i_y++ )
    {
        p_vdec->p_fifo->p_vlc->pf_memcpy( p_dst, p_src, i_width);
        p_dst += p_pic->p[0].i_pitch;
        p_src += p_ff_pic->linesize[0];
    }

    /* process each plane in a temporary buffer */
    for( i_plane = 1; i_plane < 3; i_plane++ )
    {
        i_stride = p_ff_pic->linesize[i_plane];
        i_lines = i_height / 4;

        p_dst = p_plane[i_plane] = malloc( i_lines * i_stride * 2 * 2 );
        p_src  = p_ff_pic->data[i_plane];

        /* for each source line */
        for( i_y = 0; i_y < i_lines; i_y++ )
        {
            for( i_x = 0; i_x < i_stride - 1; i_x++ )
            {
                p_dst[2 * i_x    ] = p_src[i_x];
                p_dst[2 * i_x + 1] = ( p_src[i_x] + p_src[i_x + 1]) / 2;

            }
            p_dst[2 * i_stride - 2] = p_src[i_x];
            p_dst[2 * i_stride - 1] = p_src[i_x];

            p_dst += 4 * i_stride; /* process the next even lines */
            p_src += i_stride;
        }
    }

    for( i_plane = 1; i_plane < 3; i_plane++ )
    {
        i_stride = p_ff_pic->linesize[i_plane];
        i_lines = i_height / 4;

        p_dst = p_plane[i_plane] + 2*i_stride;
        p_src  = p_plane[i_plane];

        for( i_y = 0; i_y < i_lines - 1; i_y++ )
        {
            for( i_x = 0; i_x <  2 * i_stride ; i_x++ )
            {
                p_dst[i_x] = ( p_src[i_x] + p_src[i_x + 4*i_stride])/2;
            }

            p_dst += 4 * i_stride; /* process the next odd lines */
            p_src += 4 * i_stride;
        }
        /* last line */
        p_vdec->p_fifo->p_vlc->pf_memcpy( p_dst, p_src, 2*i_stride );
    }
    /* copy to p_pic, by block
       if I do pixel per pixel it segfault. It's why I use
       temporaries buffers */
    for( i_plane = 1; i_plane < 3; i_plane++ )
    {
        int i_size;
        p_src  = p_plane[i_plane];
        p_dst = p_pic->p[i_plane].p_pixels;

        i_size = __MIN( 2*i_stride, p_pic->p[i_plane].i_pitch);
        for( i_y = 0; i_y < __MIN(p_pic->p[i_plane].i_lines, 2 * i_lines); i_y++ )
        {
            p_vdec->p_fifo->p_vlc->pf_memcpy( p_dst, p_src, i_size );
            p_src += 2 * i_stride;
            p_dst += p_pic->p[i_plane].i_pitch;
        }
        free( p_plane[i_plane] );
    }
}

/*****************************************************************************
 *
 * Functions that initialize, decode and end the decoding process
 *
 * Functions exported for ffmpeg.c
 *   * E_( InitThread_Video )
 *   * E_( DecodeThread )
 *   * E_( EndThread_Video )
 *****************************************************************************/

/*****************************************************************************
 * InitThread: initialize vdec output thread
 *****************************************************************************
 * This function is called from decoder_Run and performs the second step
 * of the initialization. It returns 0 on success. Note that the thread's
 * flag are not modified inside this function.
 *
 * ffmpeg codec will be open, some memory allocated. But Vout is not yet
 * open (done after the first decoded frame)
 *****************************************************************************/
int E_( InitThread_Video )( vdec_thread_t *p_vdec )
{
    int i_tmp;
#if LIBAVCODEC_BUILD >= 4641
    p_vdec->p_ff_pic = avcodec_alloc_picture();
#else
    p_vdec->p_ff_pic = &p_vdec->ff_pic;
#endif

    if( p_vdec->p_fifo->p_demux_data )
    {
        p_vdec->p_format = (BITMAPINFOHEADER *)p_vdec->p_fifo->p_demux_data;

        /* ***** Fill p_context with init values ***** */
        p_vdec->p_context->width  = p_vdec->p_format->biWidth;
        p_vdec->p_context->height = p_vdec->p_format->biHeight;

    }
    else
    {
        msg_Warn( p_vdec->p_fifo, "display informations missing" );
        p_vdec->p_format = NULL;
    }


    /*  ***** Get configuration of ffmpeg plugin ***** */
#if LIBAVCODEC_BUILD >= 4611
    i_tmp = config_GetInt( p_vdec->p_fifo, "ffmpeg-workaround-bugs" );
    p_vdec->p_context->workaround_bugs  = __MAX( __MIN( i_tmp, 99 ), 0 );

    i_tmp = config_GetInt( p_vdec->p_fifo, "ffmpeg-error-resilience" );
    p_vdec->p_context->error_resilience = __MAX( __MIN( i_tmp, 99 ), -1 );
#endif
#if LIBAVCODEC_BUILD >= 4614
    if( config_GetInt( p_vdec->p_fifo, "grayscale" ) )
    {
        p_vdec->p_context->flags|= CODEC_FLAG_GRAY;
    }
#endif

    p_vdec->b_hurry_up = config_GetInt(p_vdec->p_fifo, "ffmpeg-hurry-up");

    p_vdec->b_direct_rendering = 0;

#if LIBAVCODEC_BUILD >= 4641
    if( config_GetInt( p_vdec->p_fifo, "ffmpeg-dr" ) &&
        p_vdec->p_codec->capabilities & CODEC_CAP_DR1 &&
        p_vdec->p_context->pix_fmt != PIX_FMT_YUV410P ) /* <- FIXME */
    {
        msg_Dbg( p_vdec->p_fifo, "using direct rendering" );
        p_vdec->b_direct_rendering = 1;
        p_vdec->p_context->flags|= CODEC_FLAG_EMU_EDGE;
        p_vdec->p_context->get_buffer     = ffmpeg_GetFrameBuf;
        p_vdec->p_context->release_buffer = ffmpeg_ReleaseFrameBuf;
        p_vdec->p_context->opaque = p_vdec;

    }
#endif

#if 0
    /* check if codec support truncated frames */
//    if( p_vdec->p_codec->capabilities & CODEC_FLAG_TRUNCATED )
    if( p_vdec->i_codec_id == CODEC_ID_MPEG1VIDEO)
    {
        msg_Dbg( p_vdec->p_fifo, "CODEC_FLAG_TRUNCATED supported" );
        p_vdec->p_context->flags |= CODEC_FLAG_TRUNCATED;
    }
#endif

    /* ***** Open the codec ***** */
    if( avcodec_open(p_vdec->p_context, p_vdec->p_codec) < 0 )
    {
        msg_Err( p_vdec->p_fifo, "cannot open codec (%s)",
                                 p_vdec->psz_namecodec );
        return( -1 );
    }
    else
    {
        msg_Dbg( p_vdec->p_fifo, "ffmpeg codec (%s) started",
                                 p_vdec->psz_namecodec );
    }

    /* ***** init this codec with special data ***** */
    if( p_vdec->p_format &&
            p_vdec->p_format->biSize > sizeof(BITMAPINFOHEADER) )
    {
        int b_gotpicture;

        switch( p_vdec->i_codec_id )
        {
            case( CODEC_ID_MPEG4 ):
                avcodec_decode_video( p_vdec->p_context, p_vdec->p_ff_pic,
                                      &b_gotpicture,
                                      (void *)&p_vdec->p_format[1],
                                      p_vdec->p_format->biSize
                                        - sizeof(BITMAPINFOHEADER) );
                break;
            default:
                if( p_vdec->p_fifo->i_fourcc == FOURCC_MP4S ||
                    p_vdec->p_fifo->i_fourcc == FOURCC_mp4s ||
                    p_vdec->p_fifo->i_fourcc == FOURCC_M4S2 ||
                    p_vdec->p_fifo->i_fourcc == FOURCC_m4s2 ||
                    p_vdec->p_fifo->i_fourcc == FOURCC_WMV2 ||
                    p_vdec->p_fifo->i_fourcc == FOURCC_MJPG ||
                    p_vdec->p_fifo->i_fourcc == FOURCC_mjpg ||
                    p_vdec->p_fifo->i_fourcc == FOURCC_mjpa ||
                    p_vdec->p_fifo->i_fourcc == FOURCC_mjpb )
                {
                    p_vdec->p_context->extradata_size =
                        p_vdec->p_format->biSize - sizeof(BITMAPINFOHEADER);
                    p_vdec->p_context->extradata =
                        malloc( p_vdec->p_context->extradata_size );
                    memcpy( p_vdec->p_context->extradata,
                            &p_vdec->p_format[1],
                            p_vdec->p_context->extradata_size );
                }

                break;
        }
    }

    /* ***** Load post processing ***** */

    /* get overridding settings */
    p_vdec->i_pp_mode = 0;
    if( config_GetInt( p_vdec->p_fifo, "ffmpeg-db-yv" ) )
        p_vdec->i_pp_mode |= PP_DEBLOCK_Y_V;
    if( config_GetInt( p_vdec->p_fifo, "ffmpeg-db-yh" ) )
        p_vdec->i_pp_mode |= PP_DEBLOCK_Y_H;
    if( config_GetInt( p_vdec->p_fifo, "ffmpeg-db-cv" ) )
        p_vdec->i_pp_mode |= PP_DEBLOCK_C_V;
    if( config_GetInt( p_vdec->p_fifo, "ffmpeg-db-ch" ) )
        p_vdec->i_pp_mode |= PP_DEBLOCK_C_H;
    if( config_GetInt( p_vdec->p_fifo, "ffmpeg-dr-y" ) )
        p_vdec->i_pp_mode |= PP_DERING_Y;
    if( config_GetInt( p_vdec->p_fifo, "ffmpeg-dr-c" ) )
        p_vdec->i_pp_mode |= PP_DERING_C;

    if( ( config_GetInt( p_vdec->p_fifo, "ffmpeg-pp-q" ) > 0 )||
        ( config_GetInt( p_vdec->p_fifo, "ffmpeg-pp-auto" )  )||
        ( p_vdec->i_pp_mode != 0 ) )
    {
        /* check if the codec support postproc. */
        switch( p_vdec->i_codec_id )
        {
#if LIBAVCODEC_BUILD > 4608
            case( CODEC_ID_MSMPEG4V1 ):
            case( CODEC_ID_MSMPEG4V2 ):
            case( CODEC_ID_MSMPEG4V3 ):
#else
            case( CODEC_ID_MSMPEG4 ):
#endif
            case( CODEC_ID_MPEG4 ):
            case( CODEC_ID_H263 ):
//            case( CODEC_ID_H263P ): I don't use it up to now
            case( CODEC_ID_H263I ):
                /* Ok we can make postprocessing :)) */
                /* first try to get a postprocess module */
#if LIBAVCODEC_BUILD >= 4633
                p_vdec->p_pp = vlc_object_create( p_vdec->p_fifo,
                                                  sizeof( postprocessing_t ) );
                p_vdec->p_pp->psz_object_name = "postprocessing";
                p_vdec->p_pp->p_module =
                   module_Need( p_vdec->p_pp, "postprocessing", "$ffmpeg-pp" );

                if( !p_vdec->p_pp->p_module )
                {
                    msg_Warn( p_vdec->p_fifo,
                              "no suitable postprocessing module" );
                    vlc_object_destroy( p_vdec->p_pp );
                    p_vdec->p_pp = NULL;
                    p_vdec->i_pp_mode = 0;
                }
                else
                {
                    /* get mode upon quality */
                    p_vdec->i_pp_mode |=
                        p_vdec->p_pp->pf_getmode(
                              config_GetInt( p_vdec->p_fifo, "ffmpeg-pp-q" ),
                              config_GetInt( p_vdec->p_fifo, "ffmpeg-pp-auto" )
                                                );
                }
#else
                p_vdec->i_pp_mode = 0;
                msg_Warn( p_vdec->p_fifo,
                          "post-processing not supported, upgrade ffmpeg" );
#endif
                break;
            default:
                p_vdec->i_pp_mode = 0;
                msg_Warn( p_vdec->p_fifo,
                          "Post processing unsupported for this codec" );
                break;
        }

    }
//    memset( &p_vdec->statistic, 0, sizeof( statistic_t ) );

    return( 0 );
}

/*****************************************************************************
 * DecodeThread: Called to decode one frame
 *****************************************************************************
 * We have to get a frame stored in a pes, give it to ffmpeg decoder and send
 * the image to the output.
 *****************************************************************************/
void  E_( DecodeThread_Video )( vdec_thread_t *p_vdec )
{
    pes_packet_t    *p_pes;
    int     i_frame_size;
    int     i_used;
    int     b_drawpicture;
    int     b_gotpicture;
    picture_t *p_pic;                                    /* videolan picture */
    mtime_t   i_pts;

    /* TODO implement it in a better way */
    /* A good idea could be to decode all I pictures and see for the other */
    if( ( p_vdec->b_hurry_up )&& ( p_vdec->i_frame_late > 4 ) )
    {
        b_drawpicture = 0;
        if( p_vdec->i_frame_late < 8 )
        {
            p_vdec->p_context->hurry_up = 2;
        }
        else
        {
            /* too much late picture, won't decode
               but break picture until a new I, and for mpeg4 ...*/
            p_vdec->i_frame_late--; /* needed else it will never be decrease */
            input_ExtractPES( p_vdec->p_fifo, NULL );
            return;
        }
    }
    else
    {
        b_drawpicture = 1;
        p_vdec->p_context->hurry_up = 0;
    }

    do
    {
        input_ExtractPES( p_vdec->p_fifo, &p_pes );
        if( !p_pes )
        {
            p_vdec->p_fifo->b_error = 1;
            return;
        }
#if 0
        if( p_vdec->i_codec_id == CODEC_ID_MPEG1VIDEO )
        {
            if( p_pes->i_dts )
            {
                p_vdec->pts = p_pes->i_dts;
                p_vdec->i_frame_count = 0;
            }
        }
        else
        {
            if( p_pes->i_pts )
            {
                p_vdec->pts = p_pes->i_pts;
                p_vdec->i_frame_count = 0;
            }
        }
#endif
        if( p_pes->i_pts )
        {
            p_vdec->pts = p_pes->i_pts;
            p_vdec->i_frame_count = 0;
        }

        i_frame_size = p_pes->i_pes_size;

        if( i_frame_size > 0 )
        {
            uint8_t *p_last;
            int i_need;
            /* XXX Don't forget that ffmpeg required a little more bytes
             * that the real frame size */
            i_need = i_frame_size + 16 + p_vdec->i_buffer;
            if( p_vdec->i_buffer_size < i_need)
            {
                p_last = p_vdec->p_buffer;
                p_vdec->p_buffer = malloc( i_need );
                p_vdec->i_buffer_size = i_need;
                if( p_vdec->i_buffer > 0 )
                {
                    memcpy( p_vdec->p_buffer, p_last, p_vdec->i_buffer );
                }
                FREE( p_last );
            }
            i_frame_size =
                E_( GetPESData )( p_vdec->p_buffer + p_vdec->i_buffer,
                                  i_frame_size ,
                                  p_pes );
            memset( p_vdec->p_buffer + p_vdec->i_buffer + i_frame_size,
                    0,
                    16 );
        }
        input_DeletePES( p_vdec->p_fifo->p_packets_mgt, p_pes );
    } while( i_frame_size <= 0 );

    i_frame_size += p_vdec->i_buffer;

usenextdata:
    i_used = avcodec_decode_video( p_vdec->p_context,
                                   p_vdec->p_ff_pic,
                                   &b_gotpicture,
                                   p_vdec->p_buffer,
                                   i_frame_size );

#if 0
    msg_Dbg( p_vdec->p_fifo,
             "used:%d framesize:%d (%s picture)",
             i_used, i_frame_size, b_gotpicture ? "got":"no got" );
#endif
    if( i_used < 0 )
    {
        msg_Warn( p_vdec->p_fifo, "cannot decode one frame (%d bytes)",
                                  i_frame_size );
        p_vdec->i_frame_error++;
        p_vdec->i_buffer = 0;
        return;
    }
    else if( i_used < i_frame_size )
    {
#if 0
        msg_Dbg( p_vdec->p_fifo,
                 "didn't use all memory(%d  < %d)", i_used, i_frame_size );
#endif
        memmove( p_vdec->p_buffer,
                 p_vdec->p_buffer + i_used,
                 p_vdec->i_buffer_size - i_used );

        p_vdec->i_buffer = i_frame_size - i_used;
    }
    else
    {
        p_vdec->i_buffer = 0;
    }

    if( b_gotpicture )
    {
        p_vdec->i_frame_count++;
    }

    /* consumed bytes */
    i_frame_size -= i_used;

   /* Update frame late count*/
    /* I don't make statistic on decoding time */
    if( p_vdec->pts <= mdate())
    {
        p_vdec->i_frame_late++;
    }
    else
    {
        p_vdec->i_frame_late = 0;
    }

    if( !b_gotpicture || p_vdec->p_ff_pic->linesize[0] == 0 || !b_drawpicture )
    {
        return;
    }

    if( !p_vdec->b_direct_rendering )
    {
        p_vdec->p_vout = ffmpeg_CreateVout( p_vdec, p_vdec->p_context );
        if( !p_vdec->p_vout )
        {
            msg_Err( p_vdec->p_fifo, "cannot create vout" );
            p_vdec->p_fifo->b_error = 1; /* abort */
            return;
        }

        /* Get a new picture */
        while( !(p_pic = vout_CreatePicture( p_vdec->p_vout, 0, 0, 0 ) ) )
        {
            if( p_vdec->p_fifo->b_die || p_vdec->p_fifo->b_error )
            {
                return;
            }
            msleep( VOUT_OUTMEM_SLEEP );
        }

        /* fill p_picture_t from AVVideoFrame, do I410->I420 if needed */
        ffmpeg_CopyPicture( p_pic, p_vdec->p_ff_pic, p_vdec );
    }
    else
    {
#if LIBAVCODEC_BUILD >= 4641
        p_pic = (picture_t *)p_vdec->p_ff_pic->opaque;
#else
        p_pic = NULL; /*  f**ck gcc warning */
#endif
    }

    /* Do post-processing if requested */
    /* XXX: with dr it is not a good thing if the picture will be used as
       reference... */
    ffmpeg_PostProcPicture( p_vdec, p_pic );

    /* fix date calculation */
    if( p_vdec->pts > 0 )
    {
        i_pts = p_vdec->pts;

        if( p_vdec->p_context->frame_rate > 0 )
        {
           i_pts += (uint64_t)1000000 *
                    ( p_vdec->i_frame_count - 1) /
                    FRAME_RATE_BASE /
                    p_vdec->p_context->frame_rate;
        }
    }
    else
    {
        i_pts = mdate() + DEFAULT_PTS_DELAY;  // FIXME
    }

    vout_DatePicture( p_vdec->p_vout,
                      p_pic,
                      i_pts );

    /* Send decoded frame to vout */
    vout_DisplayPicture( p_vdec->p_vout, p_pic );

    if( i_frame_size > 0 )
    {
        goto usenextdata; /* try to use all data */
    }
}

/*****************************************************************************
 * EndThread: thread destruction
 *****************************************************************************
 * This function is called when the thread ends after a sucessful
 * initialization.
 *****************************************************************************/
void E_( EndThread_Video )( vdec_thread_t *p_vdec )
{

    if( p_vdec->p_pp )
    {
        /* release postprocessing module */
        module_Unneed( p_vdec->p_pp, p_vdec->p_pp->p_module );
        vlc_object_destroy( p_vdec->p_pp );
        p_vdec->p_pp = NULL;
    }

#if LIBAVCODEC_BUILD >= 4641
    if( p_vdec->p_ff_pic )
    {
        free( p_vdec->p_ff_pic );
    }
#endif

    /* We are about to die. Reattach video output to p_vlc. */
    vout_Request( p_vdec->p_fifo, p_vdec->p_vout, 0, 0, 0, 0 );
}

/*****************************************************************************
 * ffmpeg_CopyPicture: copy a picture from ffmpeg internal buffers to a
 *                     picture_t structure (when not in direct rendering mode).
 *****************************************************************************/
#if LIBAVCODEC_BUILD >= 4641
static void ffmpeg_CopyPicture( picture_t    *p_pic,
                                AVVideoFrame *p_ff_pic,
                                vdec_thread_t *p_vdec )
#else
static void ffmpeg_CopyPicture( picture_t    *p_pic,
                                AVPicture *p_ff_pic,
                                vdec_thread_t *p_vdec )
#endif
{
    int i_plane;
    int i_size;
    int i_line;

    uint8_t *p_dst;
    uint8_t *p_src;
    int i_src_stride;
    int i_dst_stride;

    if( ffmpeg_PixFmtToChroma( p_vdec->p_context->pix_fmt ) )
    {
        for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
        {
            p_src  = p_ff_pic->data[i_plane];
            p_dst = p_pic->p[i_plane].p_pixels;
            i_src_stride = p_ff_pic->linesize[i_plane];
            i_dst_stride = p_pic->p[i_plane].i_pitch;

            i_size = __MIN( i_src_stride, i_dst_stride );
            for( i_line = 0; i_line < p_pic->p[i_plane].i_lines; i_line++ )
            {
                p_vdec->p_fifo->p_vlc->pf_memcpy( p_dst, p_src, i_size );
                p_src += i_src_stride;
                p_dst += i_dst_stride;
            }
        }
    }
    else
    {
        /* we need to convert to I420 */
        switch( p_vdec->p_context->pix_fmt )
        {
#if LIBAVCODEC_BUILD >= 4615
            case( PIX_FMT_YUV410P ):
                ffmpeg_ConvertPictureI410toI420( p_pic, p_ff_pic, p_vdec );
                break;
#endif
            default:
                p_vdec->p_fifo->b_error = 1;
                break;
        }
    }
}

/*****************************************************************************
 * ffmpeg_PostProcPicture: Postprocessing is done here.
 *****************************************************************************/
static void ffmpeg_PostProcPicture( vdec_thread_t *p_vdec, picture_t *p_pic )
{
    if( ( p_vdec->i_pp_mode )&&
        ( ( p_vdec->p_vout->render.i_chroma ==
            VLC_FOURCC( 'I','4','2','0' ) )||
          ( p_vdec->p_vout->render.i_chroma ==
            VLC_FOURCC( 'Y','V','1','2' ) ) ) )
    {
#if LIBAVCODEC_BUILD >= 4641
       /* Make postproc */
        p_vdec->p_pp->pf_postprocess( p_pic,
                                      p_vdec->p_ff_pic->qscale_table,
                                      p_vdec->p_ff_pic->qstride,
                                      p_vdec->i_pp_mode );
#elif LIBAVCODEC_BUILD >= 4633
        p_vdec->p_pp->pf_postprocess( p_pic,
                                      p_vdec->p_context->display_qscale_table,
                                      p_vdec->p_context->qstride,
                                      p_vdec->i_pp_mode );
#endif
    }
}

#if LIBAVCODEC_BUILD >= 4641
/*****************************************************************************
 * ffmpeg_GetFrameBuf: callback used by ffmpeg to get a frame buffer.
 *                     (used for direct rendering)
 *****************************************************************************/

static int ffmpeg_GetFrameBuf( struct AVCodecContext *p_context,
                               AVVideoFrame *p_ff_pic )
{
    vdec_thread_t *p_vdec = (vdec_thread_t *)p_context->opaque;
    picture_t *p_pic;

    /* Check and (re)create if needed our vout */
    p_vdec->p_vout = ffmpeg_CreateVout( p_vdec, p_vdec->p_context );
    if( !p_vdec->p_vout )
    {
        msg_Err( p_vdec->p_fifo, "cannot create vout" );
        p_vdec->p_fifo->b_error = 1; /* abort */
        return -1;
    }
    p_vdec->p_vout->render.b_allow_modify_pics = 0;

    /* Get a new picture */
    while( !(p_pic = vout_CreatePicture( p_vdec->p_vout, 0, 0, 0 ) ) )
    {
        if( p_vdec->p_fifo->b_die || p_vdec->p_fifo->b_error )
        {
            return -1;
        }
        msleep( VOUT_OUTMEM_SLEEP );
    }
    p_vdec->p_context->draw_horiz_band= NULL;

    p_ff_pic->opaque = (void*)p_pic;

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
        vout_LinkPicture( p_vdec->p_vout, p_pic );
    }
    /* FIXME what is that, should give good value */
    p_ff_pic->age = 256*256*256*64; // FIXME FIXME from ffmpeg

    return( 0 );
}

static void  ffmpeg_ReleaseFrameBuf( struct AVCodecContext *p_context,
                                     AVVideoFrame *p_ff_pic )
{
    vdec_thread_t *p_vdec = (vdec_thread_t *)p_context->opaque;
    picture_t *p_pic;

    //msg_Dbg( p_vdec->p_fifo, "ffmpeg_ReleaseFrameBuf" );
    p_pic = (picture_t*)p_ff_pic->opaque;

    p_ff_pic->data[0] = NULL;
    p_ff_pic->data[1] = NULL;
    p_ff_pic->data[2] = NULL;
    p_ff_pic->data[3] = NULL;

    vout_UnlinkPicture( p_vdec->p_vout, p_pic );
}


#endif

