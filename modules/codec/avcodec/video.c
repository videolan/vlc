/*****************************************************************************
 * video.c: video decoder using the libavcodec library
 *****************************************************************************
 * Copyright (C) 1999-2001 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_codec.h>
#include <vlc_avcodec.h>
#include <vlc_cpu.h>
#include <assert.h>

#include <libavcodec/avcodec.h>
#include <libavutil/mem.h>
#include <libavutil/pixdesc.h>

#include "avcodec.h"
#include "va.h"

/*****************************************************************************
 * decoder_sys_t : decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    AVCODEC_COMMON_MEMBERS

    /* Video decoder specific part */
    mtime_t i_pts;

    AVFrame          *p_ff_pic;

    /* for frame skipping algo */
    bool b_hurry_up;
    enum AVDiscard i_skip_frame;

    /* how many decoded frames are late */
    int     i_late_frames;
    mtime_t i_late_frames_start;

    /* for direct rendering */
    bool b_direct_rendering;
    int  i_direct_rendering_used;

    bool b_has_b_frames;

    /* Hack to force display of still pictures */
    bool b_first_frame;


    /* */
    bool palette_sent;

    /* */
    bool b_flush;

    /* VA API */
    vlc_va_t *p_va;

    vlc_sem_t sem_mt;
};

#ifdef HAVE_AVCODEC_MT
#   define wait_mt(s) vlc_sem_wait( &s->sem_mt )
#   define post_mt(s) vlc_sem_post( &s->sem_mt )
#else
#   define wait_mt(s)
#   define post_mt(s)
#endif

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void ffmpeg_InitCodec      ( decoder_t * );
static void ffmpeg_CopyPicture    ( decoder_t *, picture_t *, AVFrame * );
#if LIBAVCODEC_VERSION_MAJOR >= 55
static int lavc_GetFrame(struct AVCodecContext *, AVFrame *, int);
#else
static int  ffmpeg_GetFrameBuf    ( struct AVCodecContext *, AVFrame * );
static void ffmpeg_ReleaseFrameBuf( struct AVCodecContext *, AVFrame * );
#endif
static enum PixelFormat ffmpeg_GetFormat( AVCodecContext *,
                                          const enum PixelFormat * );
static picture_t *DecodeVideo( decoder_t *, block_t ** );

static uint32_t ffmpeg_CodecTag( vlc_fourcc_t fcc )
{
    uint8_t *p = (uint8_t*)&fcc;
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

/*****************************************************************************
 * Local Functions
 *****************************************************************************/

/* Returns a new picture buffer */
static inline picture_t *ffmpeg_NewPictBuf( decoder_t *p_dec,
                                            AVCodecContext *p_context )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int width = p_context->coded_width;
    int height = p_context->coded_height;

    if( p_sys->p_va == NULL )
    {
        int aligns[AV_NUM_DATA_POINTERS];

        avcodec_align_dimensions2(p_context, &width, &height, aligns);
    }


    if( width == 0 || height == 0 || width > 8192 || height > 8192 )
    {
        msg_Err( p_dec, "Invalid frame size %dx%d.", width, height );
        return NULL; /* invalid display size */
    }
    p_dec->fmt_out.video.i_width = width;
    p_dec->fmt_out.video.i_height = height;

    if( width != p_context->width || height != p_context->height )
    {
        p_dec->fmt_out.video.i_visible_width = p_context->width;
        p_dec->fmt_out.video.i_visible_height = p_context->height;
    }
    else
    {
        p_dec->fmt_out.video.i_visible_width = width;
        p_dec->fmt_out.video.i_visible_height = height;
    }

    if( !p_sys->p_va && GetVlcChroma( &p_dec->fmt_out.video, p_context->pix_fmt ) )
    {
        /* we are doomed, but not really, because most codecs set their pix_fmt
         * much later
         * FIXME does it make sense here ? */
        p_dec->fmt_out.video.i_chroma = VLC_CODEC_I420;
    }
    p_dec->fmt_out.i_codec = p_dec->fmt_out.video.i_chroma;

    /* If an aspect-ratio was specified in the input format then force it */
    if( p_dec->fmt_in.video.i_sar_num > 0 && p_dec->fmt_in.video.i_sar_den > 0 )
    {
        p_dec->fmt_out.video.i_sar_num = p_dec->fmt_in.video.i_sar_num;
        p_dec->fmt_out.video.i_sar_den = p_dec->fmt_in.video.i_sar_den;
    }
    else
    {
        p_dec->fmt_out.video.i_sar_num = p_context->sample_aspect_ratio.num;
        p_dec->fmt_out.video.i_sar_den = p_context->sample_aspect_ratio.den;

        if( !p_dec->fmt_out.video.i_sar_num || !p_dec->fmt_out.video.i_sar_den )
        {
            p_dec->fmt_out.video.i_sar_num = 1;
            p_dec->fmt_out.video.i_sar_den = 1;
        }
    }

    if( p_dec->fmt_in.video.i_frame_rate > 0 &&
        p_dec->fmt_in.video.i_frame_rate_base > 0 )
    {
        p_dec->fmt_out.video.i_frame_rate =
            p_dec->fmt_in.video.i_frame_rate;
        p_dec->fmt_out.video.i_frame_rate_base =
            p_dec->fmt_in.video.i_frame_rate_base;
    }
    else if( p_context->time_base.num > 0 && p_context->time_base.den > 0 )
    {
        p_dec->fmt_out.video.i_frame_rate = p_context->time_base.den;
        p_dec->fmt_out.video.i_frame_rate_base = p_context->time_base.num * __MAX( p_context->ticks_per_frame, 1 );
    }

    return decoder_NewPicture( p_dec );
}

static int OpenVideoCodec( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->p_context->extradata_size <= 0 )
    {
        if( p_sys->p_codec->id == AV_CODEC_ID_VC1 ||
            p_sys->p_codec->id == AV_CODEC_ID_THEORA )
        {
            msg_Warn( p_dec, "waiting for extra data for codec %s",
                      p_sys->p_codec->name );
            return 1;
        }
    }

    p_sys->p_context->width  = p_dec->fmt_in.video.i_visible_width;
    p_sys->p_context->height = p_dec->fmt_in.video.i_visible_height;
    if (p_sys->p_context->width  == 0)
        p_sys->p_context->width  = p_dec->fmt_in.video.i_width;
    else if (p_sys->p_context->width != p_dec->fmt_in.video.i_width)
        p_sys->p_context->coded_width = p_dec->fmt_in.video.i_width;
    if (p_sys->p_context->height == 0)
        p_sys->p_context->height = p_dec->fmt_in.video.i_height;
    else if (p_sys->p_context->height != p_dec->fmt_in.video.i_height)
        p_sys->p_context->coded_height = p_dec->fmt_in.video.i_height;
    p_sys->p_context->bits_per_coded_sample = p_dec->fmt_in.video.i_bits_per_pixel;

    int ret = ffmpeg_OpenCodec( p_dec );
    if( ret < 0 )
        return ret;

#ifdef HAVE_AVCODEC_MT
    switch( p_sys->p_context->active_thread_type )
    {
        case FF_THREAD_FRAME:
            msg_Dbg( p_dec, "using frame thread mode with %d threads",
                     p_sys->p_context->thread_count );
            break;
        case FF_THREAD_SLICE:
            msg_Dbg( p_dec, "using slice thread mode with %d threads",
                     p_sys->p_context->thread_count );
            break;
        case 0:
            if( p_sys->p_context->thread_count > 1 )
                msg_Warn( p_dec, "failed to enable threaded decoding" );
            break;
        default:
            msg_Warn( p_dec, "using unknown thread mode with %d threads",
                      p_sys->p_context->thread_count );
            break;
    }
#endif
    return VLC_SUCCESS;
}

/*****************************************************************************
 * InitVideo: initialize the video decoder
 *****************************************************************************
 * the ffmpeg codec will be opened, some memory allocated. The vout is not yet
 * opened (done after the first decoded frame).
 *****************************************************************************/
int InitVideoDec( decoder_t *p_dec, AVCodecContext *p_context,
                  const AVCodec *p_codec )
{
    decoder_sys_t *p_sys;
    int i_val;

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys = calloc( 1, sizeof(decoder_sys_t) ) ) == NULL )
        return VLC_ENOMEM;

    p_sys->p_context = p_context;
    p_sys->p_codec = p_codec;
    p_sys->p_ff_pic = avcodec_alloc_frame();
    p_sys->b_delayed_open = true;
    p_sys->p_va = NULL;
    vlc_sem_init( &p_sys->sem_mt, 0 );

    /* ***** Fill p_context with init values ***** */
    p_context->codec_tag = ffmpeg_CodecTag( p_dec->fmt_in.i_original_fourcc ?
                                p_dec->fmt_in.i_original_fourcc : p_dec->fmt_in.i_codec );

    /*  ***** Get configuration of ffmpeg plugin ***** */
    p_context->workaround_bugs =
        var_InheritInteger( p_dec, "avcodec-workaround-bugs" );
    p_context->err_recognition =
        var_InheritInteger( p_dec, "avcodec-error-resilience" );

    if( var_CreateGetBool( p_dec, "grayscale" ) )
        p_context->flags |= CODEC_FLAG_GRAY;

    /* ***** Output always the frames ***** */
#if LIBAVCODEC_VERSION_CHECK(55, 23, 1, 40, 101)
    p_context->flags |= CODEC_FLAG_OUTPUT_CORRUPT;
#endif

    i_val = var_CreateGetInteger( p_dec, "avcodec-skiploopfilter" );
    if( i_val >= 4 ) p_context->skip_loop_filter = AVDISCARD_ALL;
    else if( i_val == 3 ) p_context->skip_loop_filter = AVDISCARD_NONKEY;
    else if( i_val == 2 ) p_context->skip_loop_filter = AVDISCARD_BIDIR;
    else if( i_val == 1 ) p_context->skip_loop_filter = AVDISCARD_NONREF;

    if( var_CreateGetBool( p_dec, "avcodec-fast" ) )
        p_context->flags2 |= CODEC_FLAG2_FAST;

    /* ***** libavcodec frame skipping ***** */
    p_sys->b_hurry_up = var_CreateGetBool( p_dec, "avcodec-hurry-up" );

    i_val = var_CreateGetInteger( p_dec, "avcodec-skip-frame" );
    if( i_val >= 4 ) p_context->skip_frame = AVDISCARD_ALL;
    else if( i_val == 3 ) p_context->skip_frame = AVDISCARD_NONKEY;
    else if( i_val == 2 ) p_context->skip_frame = AVDISCARD_BIDIR;
    else if( i_val == 1 ) p_context->skip_frame = AVDISCARD_NONREF;
    else if( i_val == -1 ) p_context->skip_frame = AVDISCARD_NONE;
    else p_context->skip_frame = AVDISCARD_DEFAULT;
    p_sys->i_skip_frame = p_context->skip_frame;

    i_val = var_CreateGetInteger( p_dec, "avcodec-skip-idct" );
    if( i_val >= 4 ) p_context->skip_idct = AVDISCARD_ALL;
    else if( i_val == 3 ) p_context->skip_idct = AVDISCARD_NONKEY;
    else if( i_val == 2 ) p_context->skip_idct = AVDISCARD_BIDIR;
    else if( i_val == 1 ) p_context->skip_idct = AVDISCARD_NONREF;
    else if( i_val == -1 ) p_context->skip_idct = AVDISCARD_NONE;
    else p_context->skip_idct = AVDISCARD_DEFAULT;

    /* ***** libavcodec direct rendering ***** */
    p_sys->b_direct_rendering = false;
    p_sys->i_direct_rendering_used = -1;
    if( var_CreateGetBool( p_dec, "avcodec-dr" ) &&
       (p_codec->capabilities & CODEC_CAP_DR1) &&
        /* No idea why ... but this fixes flickering on some TSCC streams */
        p_sys->p_codec->id != AV_CODEC_ID_TSCC &&
        p_sys->p_codec->id != AV_CODEC_ID_CSCD &&
        p_sys->p_codec->id != AV_CODEC_ID_CINEPAK )
    {
        /* Some codecs set pix_fmt only after the 1st frame has been decoded,
         * so we need to do another check in ffmpeg_GetFrameBuf() */
        p_sys->b_direct_rendering = true;
    }

    /* libavcodec doesn't properly release old pictures when frames are skipped */
    //if( p_sys->b_hurry_up ) p_sys->b_direct_rendering = false;
    if( p_sys->b_direct_rendering )
    {
        msg_Dbg( p_dec, "trying to use direct rendering" );
        p_context->flags |= CODEC_FLAG_EMU_EDGE;
    }
    else
    {
        msg_Dbg( p_dec, "direct rendering is disabled" );
    }

    p_context->get_format = ffmpeg_GetFormat;
    /* Always use our get_buffer wrapper so we can calculate the
     * PTS correctly */
#if LIBAVCODEC_VERSION_MAJOR >= 55
    p_context->get_buffer2 = lavc_GetFrame;
#else
    p_context->get_buffer = ffmpeg_GetFrameBuf;
    p_context->reget_buffer = avcodec_default_reget_buffer;
    p_context->release_buffer = ffmpeg_ReleaseFrameBuf;
#endif
    p_context->opaque = p_dec;

#ifdef HAVE_AVCODEC_MT
    int i_thread_count = var_InheritInteger( p_dec, "avcodec-threads" );
    if( i_thread_count <= 0 )
    {
        i_thread_count = vlc_GetCPUCount();
        if( i_thread_count > 1 )
            i_thread_count++;

        //FIXME: take in count the decoding time
        i_thread_count = __MIN( i_thread_count, 4 );
    }
    i_thread_count = __MIN( i_thread_count, 16 );
    msg_Dbg( p_dec, "allowing %d thread(s) for decoding", i_thread_count );
    p_context->thread_count = i_thread_count;
    p_context->thread_safe_callbacks = true;

    switch( p_codec->id )
    {
        case AV_CODEC_ID_MPEG4:
        case AV_CODEC_ID_H263:
            p_context->thread_type = 0;
            break;
        case AV_CODEC_ID_MPEG1VIDEO:
        case AV_CODEC_ID_MPEG2VIDEO:
            p_context->thread_type &= ~FF_THREAD_SLICE;
            /* fall through */
# if (LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55, 1, 0))
        case AV_CODEC_ID_H264:
        case AV_CODEC_ID_VC1:
        case AV_CODEC_ID_WMV3:
            p_context->thread_type &= ~FF_THREAD_FRAME;
# endif
        default:
            break;
    }

    /* Workaround: frame multithreading is not compatible with
     * DXVA2. When a frame is being copied to host memory, the frame
     * is locked and cannot be used as a reference frame
     * simultaneously and thus decoding fails for some frames. This
     * causes major image corruption. */
# if defined(_WIN32)
    char *avcodec_hw = var_InheritString( p_dec, "avcodec-hw" );
    if( avcodec_hw == NULL || strcasecmp( avcodec_hw, "none" ) )
    {
        msg_Warn( p_dec, "threaded frame decoding is not compatible with DXVA2, disabled" );
        p_context->thread_type &= ~FF_THREAD_FRAME;
    }
    free( avcodec_hw );
# endif

    if( p_context->thread_type & FF_THREAD_FRAME )
        p_dec->i_extra_picture_buffers = 2 * p_context->thread_count;
#endif

    /* ***** misc init ***** */
    p_sys->i_pts = VLC_TS_INVALID;
    p_sys->b_has_b_frames = false;
    p_sys->b_first_frame = true;
    p_sys->b_flush = false;
    p_sys->i_late_frames = 0;

    /* Set output properties */
    p_dec->fmt_out.i_cat = VIDEO_ES;
    if( GetVlcChroma( &p_dec->fmt_out.video, p_context->pix_fmt ) != VLC_SUCCESS )
    {
        /* we are doomed. but not really, because most codecs set their pix_fmt later on */
        p_dec->fmt_out.i_codec = VLC_CODEC_I420;
    }
    p_dec->fmt_out.i_codec = p_dec->fmt_out.video.i_chroma;

    p_dec->fmt_out.video.orientation = p_dec->fmt_in.video.orientation;

    if( p_dec->fmt_in.video.p_palette ) {
        p_sys->palette_sent = false;
        p_dec->fmt_out.video.p_palette = malloc( sizeof(video_palette_t) );
        if( p_dec->fmt_out.video.p_palette )
            *p_dec->fmt_out.video.p_palette = *p_dec->fmt_in.video.p_palette;
    } else
        p_sys->palette_sent = true;

    /* ***** init this codec with special data ***** */
    ffmpeg_InitCodec( p_dec );

    /* ***** Open the codec ***** */
    if( OpenVideoCodec( p_dec ) < 0 )
    {
        avcodec_free_frame( &p_sys->p_ff_pic );
        vlc_sem_destroy( &p_sys->sem_mt );
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_dec->pf_decode_video = DecodeVideo;

    if ( p_dec->fmt_in.i_codec == VLC_CODEC_VP9 )
        p_dec->b_need_packetized = true;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DecodeVideo: Called to decode one or more frames
 *****************************************************************************/
static picture_t *DecodeVideo( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    AVCodecContext *p_context = p_sys->p_context;
    int b_drawpicture;
    block_t *p_block;

    if( !pp_block )
        return NULL;

    if( !p_context->extradata_size && p_dec->fmt_in.i_extra )
    {
        ffmpeg_InitCodec( p_dec );
        if( p_sys->b_delayed_open )
            OpenVideoCodec( p_dec );
    }

    p_block = *pp_block;
    if(!p_block && !(p_sys->p_codec->capabilities & CODEC_CAP_DELAY) )
        return NULL;

    if( p_sys->b_delayed_open )
    {
        if( p_block )
            block_Release( p_block );
        return NULL;
    }

    if( p_block)
    {
        if( p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) )
        {
            p_sys->i_pts = VLC_TS_INVALID; /* To make sure we recover properly */

            p_sys->i_late_frames = 0;

            post_mt( p_sys );
            if( p_block->i_flags & BLOCK_FLAG_DISCONTINUITY )
                avcodec_flush_buffers( p_context );
            wait_mt( p_sys );

            block_Release( p_block );
            return NULL;
        }

        if( p_block->i_flags & BLOCK_FLAG_PREROLL )
        {
            /* Do not care about late frames when prerolling
             * TODO avoid decoding of non reference frame
             * (ie all B except for H264 where it depends only on nal_ref_idc) */
            p_sys->i_late_frames = 0;
        }
    }

    if( !p_dec->b_pace_control && (p_sys->i_late_frames > 0) &&
        (mdate() - p_sys->i_late_frames_start > INT64_C(5000000)) )
    {
        if( p_sys->i_pts > VLC_TS_INVALID )
        {
            p_sys->i_pts = VLC_TS_INVALID; /* To make sure we recover properly */
        }
        if( p_block )
            block_Release( p_block );
        p_sys->i_late_frames--;
        msg_Err( p_dec, "more than 5 seconds of late video -> "
                 "dropping frame (computer too slow ?)" );
        return NULL;
    }

    /* A good idea could be to decode all I pictures and see for the other */
    if( !p_dec->b_pace_control &&
        p_sys->b_hurry_up &&
        (p_sys->i_late_frames > 4) )
    {
        b_drawpicture = 0;
        if( p_sys->i_late_frames < 12 )
        {
            p_context->skip_frame =
                    (p_sys->i_skip_frame <= AVDISCARD_NONREF) ?
                    AVDISCARD_NONREF : p_sys->i_skip_frame;
        }
        else
        {
            /* picture too late, won't decode
             * but break picture until a new I, and for mpeg4 ...*/
            p_sys->i_late_frames--; /* needed else it will never be decrease */
            if( p_block )
                block_Release( p_block );
            msg_Warn( p_dec, "More than 4 late frames, dropping frame" );
            return NULL;
        }
    }
    else
    {
        if( p_sys->b_hurry_up )
            p_context->skip_frame = p_sys->i_skip_frame;
        if( !p_block || !(p_block->i_flags & BLOCK_FLAG_PREROLL) )
            b_drawpicture = 1;
        else
            b_drawpicture = 0;
    }

    if( p_context->width <= 0 || p_context->height <= 0 )
    {
        if( p_sys->b_hurry_up )
            p_context->skip_frame = p_sys->i_skip_frame;
    }
    else if( !b_drawpicture )
    {
        /* It creates broken picture
         * FIXME either our parser or ffmpeg is broken */
#if 0
        if( p_sys->b_hurry_up )
            p_context->skip_frame = __MAX( p_context->skip_frame,
                                                  AVDISCARD_NONREF );
#endif
    }

    /*
     * Do the actual decoding now */

    /* Don't forget that libavcodec requires a little more bytes
     * that the real frame size */
    if( p_block && p_block->i_buffer > 0 )
    {
        p_sys->b_flush = ( p_block->i_flags & BLOCK_FLAG_END_OF_SEQUENCE ) != 0;

        p_block = block_Realloc( p_block, 0,
                            p_block->i_buffer + FF_INPUT_BUFFER_PADDING_SIZE );
        if( !p_block )
            return NULL;
        p_block->i_buffer -= FF_INPUT_BUFFER_PADDING_SIZE;
        *pp_block = p_block;
        memset( p_block->p_buffer + p_block->i_buffer, 0,
                FF_INPUT_BUFFER_PADDING_SIZE );
    }

    while( !p_block || p_block->i_buffer > 0 || p_sys->b_flush )
    {
        int i_used, b_gotpicture;
        picture_t *p_pic;
        AVPacket pkt;

        post_mt( p_sys );

        av_init_packet( &pkt );
        if( p_block )
        {
            pkt.data = p_block->p_buffer;
            pkt.size = p_block->i_buffer;
            pkt.pts = p_block->i_pts;
            pkt.dts = p_block->i_dts;
        }
        else
        {
            /* Return delayed frames if codec has CODEC_CAP_DELAY */
            pkt.data = NULL;
            pkt.size = 0;
        }

        if( !p_sys->palette_sent )
        {
            uint8_t *pal = av_packet_new_side_data(&pkt, AV_PKT_DATA_PALETTE, AVPALETTE_SIZE);
            if (pal) {
                memcpy(pal, p_dec->fmt_in.video.p_palette->palette, AVPALETTE_SIZE);
                p_sys->palette_sent = true;
            }
        }

        /* Make sure we don't reuse the same timestamps twice */
        if( p_block )
        {
            p_block->i_pts =
            p_block->i_dts = VLC_TS_INVALID;
        }

        i_used = avcodec_decode_video2( p_context, p_sys->p_ff_pic,
                                       &b_gotpicture, &pkt );
        av_free_packet( &pkt );

        wait_mt( p_sys );

        if( p_sys->b_flush )
            p_sys->b_first_frame = true;

        if( p_block )
        {
            if( p_block->i_buffer <= 0 )
                p_sys->b_flush = false;

            if( i_used < 0 )
            {
                if( b_drawpicture )
                    msg_Warn( p_dec, "cannot decode one frame (%zu bytes)",
                            p_block->i_buffer );
                block_Release( p_block );
                return NULL;
            }
            else if( (unsigned)i_used > p_block->i_buffer ||
                    p_context->thread_count > 1 )
            {
                i_used = p_block->i_buffer;
            }

            /* Consumed bytes */
            p_block->i_buffer -= i_used;
            p_block->p_buffer += i_used;
        }

        /* Nothing to display */
        if( !b_gotpicture )
        {
            if( i_used == 0 ) break;
            continue;
        }

        /* Sanity check (seems to be needed for some streams) */
        if( p_sys->p_ff_pic->pict_type == AV_PICTURE_TYPE_B)
        {
            p_sys->b_has_b_frames = true;
        }

        /* Compute the PTS */
        mtime_t i_pts =
                    p_sys->p_ff_pic->pkt_pts;
        if (i_pts <= VLC_TS_INVALID)
            i_pts = p_sys->p_ff_pic->pkt_dts;

        if( i_pts <= VLC_TS_INVALID )
            i_pts = p_sys->i_pts;

        /* Interpolate the next PTS */
        if( i_pts > VLC_TS_INVALID )
            p_sys->i_pts = i_pts;
        if( p_sys->i_pts > VLC_TS_INVALID )
        {
            /* interpolate the next PTS */
            if( p_dec->fmt_in.video.i_frame_rate > 0 &&
                p_dec->fmt_in.video.i_frame_rate_base > 0 )
            {
                p_sys->i_pts += CLOCK_FREQ *
                    (2 + p_sys->p_ff_pic->repeat_pict) *
                    p_dec->fmt_in.video.i_frame_rate_base /
                    (2 * p_dec->fmt_in.video.i_frame_rate);
            }
            else if( p_context->time_base.den > 0 )
            {
                int i_tick = p_context->ticks_per_frame;
                if( i_tick <= 0 )
                    i_tick = 1;

                p_sys->i_pts += CLOCK_FREQ *
                    (2 + p_sys->p_ff_pic->repeat_pict) *
                    i_tick * p_context->time_base.num /
                    (2 * p_context->time_base.den);
            }
        }

        /* Update frame late count (except when doing preroll) */
        mtime_t i_display_date = 0;
        if( !p_block || !(p_block->i_flags & BLOCK_FLAG_PREROLL) )
            i_display_date = decoder_GetDisplayDate( p_dec, i_pts );

        if( i_display_date > 0 && i_display_date <= mdate() )
        {
            p_sys->i_late_frames++;
            if( p_sys->i_late_frames == 1 )
                p_sys->i_late_frames_start = mdate();
        }
        else
        {
            p_sys->i_late_frames = 0;
        }

        if( !b_drawpicture || ( !p_sys->p_va && !p_sys->p_ff_pic->linesize[0] ) )
            continue;

        if( p_sys->p_va != NULL || p_sys->p_ff_pic->opaque == NULL )
        {
            /* Get a new picture */
            p_pic = ffmpeg_NewPictBuf( p_dec, p_context );
            if( !p_pic )
            {
                if( p_block )
                    block_Release( p_block );
                return NULL;
            }

            /* Fill p_picture_t from AVVideoFrame and do chroma conversion
             * if needed */
            ffmpeg_CopyPicture( p_dec, p_pic, p_sys->p_ff_pic );
        }
        else
        {
            p_pic = (picture_t *)p_sys->p_ff_pic->opaque;
            picture_Hold( p_pic );
        }

        if( !p_dec->fmt_in.video.i_sar_num || !p_dec->fmt_in.video.i_sar_den )
        {
            /* Fetch again the aspect ratio in case it changed */
            p_dec->fmt_out.video.i_sar_num
                = p_context->sample_aspect_ratio.num;
            p_dec->fmt_out.video.i_sar_den
                = p_context->sample_aspect_ratio.den;

            if( !p_dec->fmt_out.video.i_sar_num || !p_dec->fmt_out.video.i_sar_den )
            {
                p_dec->fmt_out.video.i_sar_num = 1;
                p_dec->fmt_out.video.i_sar_den = 1;
            }
        }

        /* Send decoded frame to vout */
        if( i_pts > VLC_TS_INVALID)
        {
            p_pic->date = i_pts;

            if( p_sys->b_first_frame )
            {
                /* Hack to force display of still pictures */
                p_sys->b_first_frame = false;
                p_pic->b_force = true;
            }

            p_pic->i_nb_fields = 2 + p_sys->p_ff_pic->repeat_pict;
            p_pic->b_progressive = !p_sys->p_ff_pic->interlaced_frame;
            p_pic->b_top_field_first = p_sys->p_ff_pic->top_field_first;

            return p_pic;
        }
        else
        {
            picture_Release( p_pic );
        }
    }

    if( p_block )
        block_Release( p_block );
    return NULL;
}

/*****************************************************************************
 * EndVideo: decoder destruction
 *****************************************************************************
 * This function is called when the thread ends after a successful
 * initialization.
 *****************************************************************************/
void EndVideoDec( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    post_mt( p_sys );

    /* do not flush buffers if codec hasn't been opened (theora/vorbis/VC1) */
    if( p_sys->p_context->codec )
        avcodec_flush_buffers( p_sys->p_context );

    wait_mt( p_sys );

    ffmpeg_CloseCodec( p_dec );

    if( p_sys->p_ff_pic )
        avcodec_free_frame( &p_sys->p_ff_pic );

    if( p_sys->p_va )
        vlc_va_Delete( p_sys->p_va, p_sys->p_context );

    vlc_sem_destroy( &p_sys->sem_mt );
}

/*****************************************************************************
 * ffmpeg_InitCodec: setup codec extra initialization data for ffmpeg
 *****************************************************************************/
static void ffmpeg_InitCodec( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int i_size = p_dec->fmt_in.i_extra;

    if( !i_size ) return;

    if( p_sys->p_codec->id == AV_CODEC_ID_SVQ3 )
    {
        uint8_t *p;

        p_sys->p_context->extradata_size = i_size + 12;
        p = p_sys->p_context->extradata =
            av_malloc( p_sys->p_context->extradata_size +
                       FF_INPUT_BUFFER_PADDING_SIZE );
        if( !p )
            return;

        memcpy( &p[0],  "SVQ3", 4 );
        memset( &p[4], 0, 8 );
        memcpy( &p[12], p_dec->fmt_in.p_extra, i_size );

        /* Now remove all atoms before the SMI one */
        if( p_sys->p_context->extradata_size > 0x5a &&
            strncmp( (char*)&p[0x56], "SMI ", 4 ) )
        {
            uint8_t *psz = &p[0x52];

            while( psz < &p[p_sys->p_context->extradata_size - 8] )
            {
                int i_size = GetDWBE( psz );
                if( i_size <= 1 )
                {
                    /* FIXME handle 1 as long size */
                    break;
                }
                if( !strncmp( (char*)&psz[4], "SMI ", 4 ) )
                {
                    memmove( &p[0x52], psz,
                             &p[p_sys->p_context->extradata_size] - psz );
                    break;
                }

                psz += i_size;
            }
        }
    }
    else
    {
        p_sys->p_context->extradata_size = i_size;
        p_sys->p_context->extradata =
            av_malloc( i_size + FF_INPUT_BUFFER_PADDING_SIZE );
        if( p_sys->p_context->extradata )
        {
            memcpy( p_sys->p_context->extradata,
                    p_dec->fmt_in.p_extra, i_size );
            memset( p_sys->p_context->extradata + i_size,
                    0, FF_INPUT_BUFFER_PADDING_SIZE );
        }
    }
}

/*****************************************************************************
 * ffmpeg_CopyPicture: copy a picture from ffmpeg internal buffers to a
 *                     picture_t structure (when not in direct rendering mode).
 *****************************************************************************/
static void ffmpeg_CopyPicture( decoder_t *p_dec,
                                picture_t *p_pic, AVFrame *p_ff_pic )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->p_va )
    {
        vlc_va_Extract( p_sys->p_va, p_pic, p_ff_pic->opaque,
                        p_ff_pic->data[3] );
    }
    else if( FindVlcChroma( p_sys->p_context->pix_fmt ) )
    {
        int i_plane, i_size, i_line;
        uint8_t *p_dst, *p_src;
        int i_src_stride, i_dst_stride;

        if( p_sys->p_context->pix_fmt == PIX_FMT_PAL8 )
        {
            if( !p_pic->format.p_palette )
                p_pic->format.p_palette = calloc( 1, sizeof(video_palette_t) );

            if( p_pic->format.p_palette )
            {
                p_pic->format.p_palette->i_entries = AVPALETTE_COUNT;
                memcpy( p_pic->format.p_palette->palette, p_ff_pic->data[1], AVPALETTE_SIZE );
            }
        }

        for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
        {
            p_src  = p_ff_pic->data[i_plane];
            p_dst = p_pic->p[i_plane].p_pixels;
            i_src_stride = p_ff_pic->linesize[i_plane];
            i_dst_stride = p_pic->p[i_plane].i_pitch;

            i_size = __MIN( i_src_stride, i_dst_stride );
            for( i_line = 0; i_line < p_pic->p[i_plane].i_visible_lines;
                 i_line++ )
            {
                memcpy( p_dst, p_src, i_size );
                p_src += i_src_stride;
                p_dst += i_dst_stride;
            }
        }
    }
    else
    {
        const char *name = av_get_pix_fmt_name( p_sys->p_context->pix_fmt );
        msg_Err( p_dec, "Unsupported decoded output format %d (%s)",
                 p_sys->p_context->pix_fmt, name ? name : "unknown" );
        p_dec->b_error = 1;
    }
}

#if LIBAVCODEC_VERSION_MAJOR >= 55
static int lavc_va_GetFrame(struct AVCodecContext *ctx, AVFrame *frame,
                            int flags)
{
    decoder_t *dec = ctx->opaque;
    decoder_sys_t *sys = dec->p_sys;
    vlc_va_t *va = sys->p_va;

    if (vlc_va_Setup(va, ctx, &dec->fmt_out.video.i_chroma))
    {
        msg_Err(dec, "hardware acceleration setup failed");
        return -1;
    }
    if (vlc_va_Get(va, &frame->opaque, &frame->data[0]))
    {
        msg_Err(dec, "hardware acceleration picture allocation failed");
        return -1;
    }
    /* data[0] must be non-NULL for libavcodec internal checks.
     * data[3] actually contains the format-specific surface handle. */
    frame->data[3] = frame->data[0];

    frame->buf[0] = av_buffer_create(frame->data[0], 0, va->release,
                                     frame->opaque, 0);
    if (unlikely(frame->buf[0] == NULL))
    {
        vlc_va_Release(va, frame->opaque, frame->data[0]);
        return -1;
    }
    assert(frame->data[0] != NULL);
    (void) flags;
    return 0;
}

static void lavc_dr_ReleaseFrame(void *opaque, uint8_t *data)
{
    picture_t *picture = opaque;

    picture_Release(picture);
    (void) data;
}

static picture_t *lavc_dr_GetFrame(struct AVCodecContext *ctx,
                                   AVFrame *frame, int flags)
{
    decoder_t *dec = (decoder_t *)ctx->opaque;
    decoder_sys_t *sys = dec->p_sys;

    if (GetVlcChroma(&dec->fmt_out.video, ctx->pix_fmt) != VLC_SUCCESS)
        return NULL;
    dec->fmt_out.i_codec = dec->fmt_out.video.i_chroma;
    if (ctx->pix_fmt == PIX_FMT_PAL8)
        return NULL;

    int width = frame->width;
    int height = frame->height;
    int aligns[AV_NUM_DATA_POINTERS];

    avcodec_align_dimensions2(ctx, &width, &height, aligns);

    picture_t *pic = ffmpeg_NewPictBuf(dec, ctx);
    if (pic == NULL)
        return NULL;

    /* Check that the picture is suitable for libavcodec */
    if (pic->p[0].i_pitch < width * pic->p[0].i_pixel_pitch)
    {
        if (sys->i_direct_rendering_used != 0)
            msg_Dbg(dec, "plane 0: pitch too small (%d/%d*%d)",
                    pic->p[0].i_pitch, width, pic->p[0].i_pixel_pitch);
        goto no_dr;
    }

    if (pic->p[0].i_lines < height)
    {
        if (sys->i_direct_rendering_used != 0)
            msg_Dbg(dec, "plane 0: lines too few (%d/%d)",
                    pic->p[0].i_lines, height);
        goto no_dr;
    }

    for (int i = 0; i < pic->i_planes; i++)
    {
        if (pic->p[i].i_pitch % aligns[i])
        {
            if (sys->i_direct_rendering_used != 0)
                msg_Dbg(dec, "plane %d: pitch not aligned (%d%%%d)",
                        i, pic->p[i].i_pitch, aligns[i]);
            goto no_dr;
        }
        if (((uintptr_t)pic->p[i].p_pixels) % aligns[i])
        {
            if (sys->i_direct_rendering_used != 0)
                msg_Warn(dec, "plane %d not aligned", i);
            goto no_dr;
        }
    }

    /* Allocate buffer references */
    for (int i = 0; i < pic->i_planes; i++)
    {
        uint8_t *data = pic->p[i].p_pixels;
        int size = pic->p[i].i_pitch * pic->p[i].i_lines;

        frame->buf[i] = av_buffer_create(data, size, lavc_dr_ReleaseFrame,
                                         picture_Hold(pic), 0);
        if (unlikely(frame->buf[i] == NULL))
        {
            lavc_dr_ReleaseFrame(pic, data);
            goto error;
        }
    }
    picture_Release(pic);
    (void) flags;
    return pic;
error:
    for (unsigned i = 0; frame->buf[i] != NULL; i++)
        av_buffer_unref(&frame->buf[i]);
no_dr:
    picture_Release(pic);
    return NULL;
}

/**
 * Callback used by libavcodec to get a frame buffer.
 *
 * It is used for direct rendering as well as to get the right PTS for each
 * decoded picture (even in indirect rendering mode).
 */
static int lavc_GetFrame(struct AVCodecContext *ctx, AVFrame *frame, int flags)
{
    decoder_t *dec = ctx->opaque;
    decoder_sys_t *sys = dec->p_sys;
    picture_t *pic;

    for (unsigned i = 0; i < AV_NUM_DATA_POINTERS; i++)
    {
        frame->data[i] = NULL;
        frame->linesize[i] = 0;
        frame->buf[i] = NULL;
    }

    if (sys->p_va != NULL)
        return lavc_va_GetFrame(ctx, frame, flags);

    frame->opaque = NULL;
    if (!sys->b_direct_rendering)
        return avcodec_default_get_buffer2(ctx, frame, flags);

    /* Some codecs set pix_fmt only after the 1st frame has been decoded,
     * so we need to check for direct rendering again. */
    wait_mt(sys);
    pic = lavc_dr_GetFrame(ctx, frame, flags);
    if (pic == NULL)
    {
        if (sys->i_direct_rendering_used != 0)
        {
            msg_Warn(dec, "disabling direct rendering");
            sys->i_direct_rendering_used = 0;
        }
        post_mt(sys);
        return avcodec_default_get_buffer2(ctx, frame, flags);
    }

    if (sys->i_direct_rendering_used != 1)
    {
        msg_Dbg(dec, "enabling direct rendering");
        sys->i_direct_rendering_used = 1;
    }
    post_mt(sys);

    frame->opaque = pic;
    static_assert(PICTURE_PLANE_MAX <= AV_NUM_DATA_POINTERS, "Oops!");
    for (unsigned i = 0; i < PICTURE_PLANE_MAX; i++)
    {
        frame->data[i] = pic->p[i].p_pixels;
        frame->linesize[i] = pic->p[i].i_pitch;
    }
    return 0;
}
#else
static int ffmpeg_va_GetFrameBuf( struct AVCodecContext *p_context, AVFrame *p_ff_pic )
{
    decoder_t *p_dec = (decoder_t *)p_context->opaque;
    decoder_sys_t *p_sys = p_dec->p_sys;
    vlc_va_t *p_va = p_sys->p_va;

    /* hwaccel_context is not present in old ffmpeg version */
    if( vlc_va_Setup( p_va, p_context, &p_dec->fmt_out.video.i_chroma ) )
    {
        msg_Err( p_dec, "vlc_va_Setup failed" );
        return -1;
    }

    if( vlc_va_Get( p_va, &p_ff_pic->opaque, &p_ff_pic->data[0] ) )
    {
        msg_Err( p_dec, "vlc_va_Get failed" );
        return -1;
    }

    p_ff_pic->data[3] = p_ff_pic->data[0];
    p_ff_pic->type = FF_BUFFER_TYPE_USER;
    return 0;
}

static picture_t *ffmpeg_dr_GetFrameBuf(struct AVCodecContext *p_context)
{
    decoder_t *p_dec = (decoder_t *)p_context->opaque;

    int i_width = p_context->width;
    int i_height = p_context->height;
    avcodec_align_dimensions( p_context, &i_width, &i_height );

    picture_t *p_pic = NULL;
    if (GetVlcChroma(&p_dec->fmt_out.video, p_context->pix_fmt) != VLC_SUCCESS)
        goto no_dr;

    if (p_context->pix_fmt == PIX_FMT_PAL8)
        goto no_dr;

    p_dec->fmt_out.i_codec = p_dec->fmt_out.video.i_chroma;

    p_pic = ffmpeg_NewPictBuf( p_dec, p_context );
    if( !p_pic )
        goto no_dr;

    if( p_pic->p[0].i_pitch / p_pic->p[0].i_pixel_pitch < i_width ||
        p_pic->p[0].i_lines < i_height )
        goto no_dr;

    for( int i = 0; i < p_pic->i_planes; i++ )
    {
        unsigned i_align;
        switch( p_context->codec_id )
        {
        case AV_CODEC_ID_SVQ1:
        case AV_CODEC_ID_VP5:
        case AV_CODEC_ID_VP6:
        case AV_CODEC_ID_VP6F:
        case AV_CODEC_ID_VP6A:
            i_align = 16;
            break;
        default:
            i_align = i == 0 ? 16 : 8;
            break;
        }
        if( p_pic->p[i].i_pitch % i_align )
            goto no_dr;
        if( (intptr_t)p_pic->p[i].p_pixels % i_align )
            goto no_dr;
    }

    if( p_context->pix_fmt == PIX_FMT_YUV422P )
    {
        if( 2 * p_pic->p[1].i_pitch != p_pic->p[0].i_pitch ||
            2 * p_pic->p[2].i_pitch != p_pic->p[0].i_pitch )
            goto no_dr;
    }

    return p_pic;

no_dr:
    if (p_pic)
        picture_Release( p_pic );

    return NULL;
}

static int ffmpeg_GetFrameBuf( struct AVCodecContext *p_context,
                               AVFrame *p_ff_pic )
{
    decoder_t *p_dec = (decoder_t *)p_context->opaque;
    decoder_sys_t *p_sys = p_dec->p_sys;

    /* */
    p_ff_pic->opaque = NULL;
#if ! LIBAVCODEC_VERSION_CHECK(54, 34, 0, 79, 101)
    p_ff_pic->pkt_pts = p_context->pkt ? p_context->pkt->pts : AV_NOPTS_VALUE;
#endif

    if( p_sys->p_va )
        return ffmpeg_va_GetFrameBuf(p_context, p_ff_pic);

    if( !p_sys->b_direct_rendering )
        return avcodec_default_get_buffer( p_context, p_ff_pic );

    wait_mt( p_sys );
    /* Some codecs set pix_fmt only after the 1st frame has been decoded,
     * so we need to check for direct rendering again. */

    picture_t *p_pic = ffmpeg_dr_GetFrameBuf(p_context);
    if (!p_pic) {
        if( p_sys->i_direct_rendering_used != 0 )
        {
            msg_Warn( p_dec, "disabling direct rendering" );
            p_sys->i_direct_rendering_used = 0;
        }

        post_mt( p_sys );
        return avcodec_default_get_buffer( p_context, p_ff_pic );
    }

    if( p_sys->i_direct_rendering_used != 1 ) {
        msg_Dbg( p_dec, "using direct rendering" );
        p_sys->i_direct_rendering_used = 1;
    }

    p_context->draw_horiz_band = NULL;
    post_mt( p_sys );

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

    return 0;
}

static void ffmpeg_ReleaseFrameBuf( struct AVCodecContext *p_context,
                                    AVFrame *p_ff_pic )
{
    decoder_t *p_dec = (decoder_t *)p_context->opaque;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->p_va )
        vlc_va_Release( p_sys->p_va, p_ff_pic->opaque, p_ff_pic->data[0] );
    else if( p_ff_pic->opaque )
        picture_Release( (picture_t*)p_ff_pic->opaque);
    else if( p_ff_pic->type == FF_BUFFER_TYPE_INTERNAL )
        /* We can end up here without the AVFrame being allocated by
         * avcodec_default_get_buffer() if VA is used and the frame is
         * released when the decoder is closed
         */
        avcodec_default_release_buffer( p_context, p_ff_pic );

    for( int i = 0; i < 4; i++ )
        p_ff_pic->data[i] = NULL;
}
#endif

static enum PixelFormat ffmpeg_GetFormat( AVCodecContext *p_context,
                                          const enum PixelFormat *pi_fmt )
{
    decoder_t *p_dec = p_context->opaque;
    decoder_sys_t *p_sys = p_dec->p_sys;
    vlc_va_t *p_va = p_sys->p_va;

    if( p_va != NULL )
        vlc_va_Delete( p_va, p_context );

    /* Enumerate available formats */
    bool can_hwaccel = false;
    for( size_t i = 0; pi_fmt[i] != PIX_FMT_NONE; i++ )
    {
        const AVPixFmtDescriptor *dsc = av_pix_fmt_desc_get(pi_fmt[i]);
        if (dsc == NULL)
            continue;
        bool hwaccel = (dsc->flags & AV_PIX_FMT_FLAG_HWACCEL) != 0;

        msg_Dbg( p_dec, "available %sware decoder output format %d (%s)",
                 hwaccel ? "hard" : "soft", pi_fmt[i], dsc->name );
        if (hwaccel)
            can_hwaccel = true;
    }

    if (!can_hwaccel)
        goto end;

    /* Profile and level information is needed now.
     * TODO: avoid code duplication with avcodec.c */
    if( p_context->profile != FF_PROFILE_UNKNOWN)
        p_dec->fmt_in.i_profile = p_context->profile;
    if( p_context->level != FF_LEVEL_UNKNOWN)
        p_dec->fmt_in.i_level = p_context->level;

    p_va = vlc_va_New( VLC_OBJECT(p_dec), p_context, &p_dec->fmt_in );
    if( p_va == NULL )
        goto end;

    for( size_t i = 0; pi_fmt[i] != PIX_FMT_NONE; i++ )
    {
        if( p_va->pix_fmt != pi_fmt[i] )
            continue;

        /* We try to call vlc_va_Setup when possible to detect errors when
         * possible (later is too late) */
        if( p_context->width > 0 && p_context->height > 0
         && vlc_va_Setup( p_va, p_context, &p_dec->fmt_out.video.i_chroma ) )
        {
            msg_Err( p_dec, "acceleration setup failure" );
            break;
        }

        if( p_va->description )
            msg_Info( p_dec, "Using %s for hardware decoding.",
                      p_va->description );

        /* FIXME this will disable direct rendering
         * even if a new pixel format is renegotiated
         */
        p_sys->b_direct_rendering = false;
        p_sys->p_va = p_va;
        p_context->draw_horiz_band = NULL;
        return pi_fmt[i];
    }

    vlc_va_Delete( p_va, p_context );

end:
    /* Fallback to default behaviour */
    p_sys->p_va = NULL;
    return avcodec_default_get_format( p_context, pi_fmt );
}
