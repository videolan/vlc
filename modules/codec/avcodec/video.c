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
#include <vlc_atomic.h>
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

    /* for frame skipping algo */
    bool b_hurry_up;
    enum AVDiscard i_skip_frame;

    /* how many decoded frames are late */
    int     i_late_frames;
    mtime_t i_late_frames_start;

    /* for direct rendering */
    bool        b_direct_rendering;
    atomic_bool b_dr_failure;

    /* Hack to force display of still pictures */
    bool b_first_frame;


    /* */
    bool palette_sent;

    /* */
    bool b_flush;

    /* VA API */
    vlc_va_t *p_va;
    enum PixelFormat pix_fmt;
    int profile;
    int level;

    vlc_sem_t sem_mt;
};

#ifdef HAVE_AVCODEC_MT
static inline void wait_mt(decoder_sys_t *sys)
{
    vlc_sem_wait(&sys->sem_mt);
}

static inline void post_mt(decoder_sys_t *sys)
{
    vlc_sem_post(&sys->sem_mt);
}
#else
# define wait_mt(s) ((void)s)
# define post_mt(s) ((void)s)
#endif

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void ffmpeg_InitCodec      ( decoder_t * );
static int lavc_GetFrame(struct AVCodecContext *, AVFrame *, int);
static enum PixelFormat ffmpeg_GetFormat( AVCodecContext *,
                                          const enum PixelFormat * );
static picture_t *DecodeVideo( decoder_t *, block_t ** );
static void Flush( decoder_t * );

static uint32_t ffmpeg_CodecTag( vlc_fourcc_t fcc )
{
    uint8_t *p = (uint8_t*)&fcc;
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

/*****************************************************************************
 * Local Functions
 *****************************************************************************/

/**
 * Sets the decoder output format.
 */
static int lavc_GetVideoFormat(decoder_t *dec, video_format_t *restrict fmt,
                               AVCodecContext *ctx, enum AVPixelFormat pix_fmt,
                               enum AVPixelFormat sw_pix_fmt)
{
    int width = ctx->coded_width;
    int height = ctx->coded_height;

    video_format_Init(fmt, 0);

    if (pix_fmt == sw_pix_fmt)
    {   /* software decoding */
        int aligns[AV_NUM_DATA_POINTERS];

        if (GetVlcChroma(fmt, pix_fmt))
            return -1;

        avcodec_align_dimensions2(ctx, &width, &height, aligns);
    }
    else /* hardware decoding */
        fmt->i_chroma = vlc_va_GetChroma(pix_fmt, sw_pix_fmt);

    if( width == 0 || height == 0 || width > 8192 || height > 8192 )
    {
        msg_Err(dec, "Invalid frame size %dx%d.", width, height);
        return -1; /* invalid display size */
    }

    fmt->i_width = width;
    fmt->i_height = height;
    fmt->i_visible_width = ctx->width;
    fmt->i_visible_height = ctx->height;

    /* If an aspect-ratio was specified in the input format then force it */
    if (dec->fmt_in.video.i_sar_num > 0 && dec->fmt_in.video.i_sar_den > 0)
    {
        fmt->i_sar_num = dec->fmt_in.video.i_sar_num;
        fmt->i_sar_den = dec->fmt_in.video.i_sar_den;
    }
    else
    {
        fmt->i_sar_num = ctx->sample_aspect_ratio.num;
        fmt->i_sar_den = ctx->sample_aspect_ratio.den;

        if (fmt->i_sar_num == 0 || fmt->i_sar_den == 0)
            fmt->i_sar_num = fmt->i_sar_den = 1;
    }

    if (dec->fmt_in.video.i_frame_rate > 0
     && dec->fmt_in.video.i_frame_rate_base > 0)
    {
        fmt->i_frame_rate = dec->fmt_in.video.i_frame_rate;
        fmt->i_frame_rate_base = dec->fmt_in.video.i_frame_rate_base;
    }
#if LIBAVCODEC_VERSION_CHECK( 56, 5, 0, 7, 100 )
    else if (ctx->framerate.num > 0 && ctx->framerate.den > 0)
    {
        fmt->i_frame_rate = ctx->framerate.num;
        fmt->i_frame_rate_base = ctx->framerate.den;
# if LIBAVCODEC_VERSION_MICRO <  100
        // for some reason libav don't thinkg framerate presents actually same thing as in ffmpeg
        fmt->i_frame_rate_base *= __MAX(ctx->ticks_per_frame, 1);
# endif
    }
#endif
    else if (ctx->time_base.num > 0 && ctx->time_base.den > 0)
    {
        fmt->i_frame_rate = ctx->time_base.den;
        fmt->i_frame_rate_base = ctx->time_base.num
                                 * __MAX(ctx->ticks_per_frame, 1);
    }

    if( ctx->color_range == AVCOL_RANGE_JPEG )
        fmt->b_color_range_full = true;

    switch( ctx->colorspace )
    {
        case AVCOL_SPC_BT709:
            fmt->space = COLOR_SPACE_BT709;
            break;
        case AVCOL_SPC_SMPTE170M:
        case AVCOL_SPC_BT470BG:
            fmt->space = COLOR_SPACE_BT601;
            break;
        case AVCOL_SPC_BT2020_NCL:
        case AVCOL_SPC_BT2020_CL:
            fmt->space = COLOR_SPACE_BT2020;
            break;
        default:
            break;
    }

    switch( ctx->color_trc )
    {
        case AVCOL_TRC_LINEAR:
            fmt->transfer = TRANSFER_FUNC_LINEAR;
            break;
        case AVCOL_TRC_GAMMA22:
            fmt->transfer = TRANSFER_FUNC_SRGB;
            break;
        case AVCOL_TRC_BT709:
            fmt->transfer = TRANSFER_FUNC_BT709;
            break;
        case AVCOL_TRC_SMPTE170M:
        case AVCOL_TRC_BT2020_10:
        case AVCOL_TRC_BT2020_12:
            fmt->transfer = TRANSFER_FUNC_BT2020;
            break;
        default:
            break;
    }

    switch( ctx->color_primaries )
    {
        case AVCOL_PRI_BT709:
            fmt->primaries = COLOR_PRIMARIES_BT709;
            break;
        case AVCOL_PRI_BT470BG:
            fmt->primaries = COLOR_PRIMARIES_BT601_625;
            break;
        case AVCOL_PRI_SMPTE170M:
        case AVCOL_PRI_SMPTE240M:
            fmt->primaries = COLOR_PRIMARIES_BT601_525;
            break;
        case AVCOL_PRI_BT2020:
            fmt->primaries = COLOR_PRIMARIES_BT2020;
            break;
        default:
            break;
    }

    switch( ctx->chroma_sample_location )
    {
        case AVCHROMA_LOC_LEFT:
            fmt->chroma_location = CHROMA_LOCATION_LEFT;
            break;
        case AVCHROMA_LOC_CENTER:
            fmt->chroma_location = CHROMA_LOCATION_CENTER;
            break;
        case AVCHROMA_LOC_TOPLEFT:
            fmt->chroma_location = CHROMA_LOCATION_TOP_LEFT;
            break;
        default:
            break;
    }

    return 0;
}

static int lavc_UpdateVideoFormat(decoder_t *dec, AVCodecContext *ctx,
                                  enum AVPixelFormat fmt,
                                  enum AVPixelFormat swfmt)
{
    video_format_t fmt_out;
    int val;

    val = lavc_GetVideoFormat(dec, &fmt_out, ctx, fmt, swfmt);
    if (val)
        return val;

    es_format_Clean(&dec->fmt_out);
    es_format_Init(&dec->fmt_out, VIDEO_ES, fmt_out.i_chroma);
    dec->fmt_out.video = fmt_out;
    dec->fmt_out.video.orientation = dec->fmt_in.video.orientation;;
    return decoder_UpdateVideoFormat(dec);
}

/**
 * Copies a picture from the libavcodec-allocate buffer to a picture_t.
 * This is used when not in direct rendering mode.
 */
static void lavc_CopyPicture(decoder_t *dec, picture_t *pic, AVFrame *frame)
{
    decoder_sys_t *sys = dec->p_sys;

    if (!FindVlcChroma(sys->p_context->pix_fmt))
    {
        const char *name = av_get_pix_fmt_name(sys->p_context->pix_fmt);

        msg_Err(dec, "Unsupported decoded output format %d (%s)",
                sys->p_context->pix_fmt, (name != NULL) ? name : "unknown");
        dec->b_error = true;
        return;
    }

    for (int plane = 0; plane < pic->i_planes; plane++)
    {
        const uint8_t *src = frame->data[plane];
        uint8_t *dst = pic->p[plane].p_pixels;
        size_t src_stride = frame->linesize[plane];
        size_t dst_stride = pic->p[plane].i_pitch;
        size_t size = __MIN(src_stride, dst_stride);

        for (int line = 0; line < pic->p[plane].i_visible_lines; line++)
        {
            memcpy(dst, src, size);
            src += src_stride;
            dst += dst_stride;
        }
    }
}

static int OpenVideoCodec( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int ret;

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
    p_sys->pix_fmt = AV_PIX_FMT_NONE;
    p_sys->profile = -1;
    p_sys->level = -1;

    post_mt( p_sys );
    ret = ffmpeg_OpenCodec( p_dec );
    wait_mt( p_sys );
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
    return 0;
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
    p_context->flags |= CODEC_FLAG_OUTPUT_CORRUPT;

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
    atomic_init(&p_sys->b_dr_failure, false);
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

#if !LIBAVCODEC_VERSION_CHECK(55, 32, 1, 48, 102)
    p_context->flags |= CODEC_FLAG_EMU_EDGE;
#endif
    p_context->get_format = ffmpeg_GetFormat;
    /* Always use our get_buffer wrapper so we can calculate the
     * PTS correctly */
    p_context->get_buffer2 = lavc_GetFrame;
    p_context->refcounted_frames = true;
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

    if( p_context->thread_type & FF_THREAD_FRAME )
        p_dec->i_extra_picture_buffers = 2 * p_context->thread_count;
#endif

    /* ***** misc init ***** */
    p_sys->i_pts = VLC_TS_INVALID;
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
        vlc_sem_destroy( &p_sys->sem_mt );
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_dec->pf_decode_video = DecodeVideo;
    p_dec->pf_flush        = Flush;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Flush:
 *****************************************************************************/
static void Flush( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    AVCodecContext *p_context = p_sys->p_context;

    p_sys->i_pts = VLC_TS_INVALID; /* To make sure we recover properly */
    p_sys->i_late_frames = 0;

    /* Abort pictures in order to unblock all avcodec workers threads waiting
     * for a picture. This will avoid a deadlock between avcodec_flush_buffers
     * and workers threads */
    decoder_AbortPictures( p_dec, true );

    post_mt( p_sys );
    avcodec_flush_buffers( p_context );
    wait_mt( p_sys );

    /* Reset cancel state to false */
    decoder_AbortPictures( p_dec, false );
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

    if( !p_context->extradata_size && p_dec->fmt_in.i_extra )
    {
        ffmpeg_InitCodec( p_dec );
        if( p_sys->b_delayed_open )
            OpenVideoCodec( p_dec );
    }

    p_block = pp_block ? *pp_block : NULL;
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
            if( p_block->i_flags & BLOCK_FLAG_CORRUPTED )
            {
                block_Release( p_block );
                return NULL;
            }
        }

        if( p_block->i_flags & BLOCK_FLAG_PREROLL )
        {
            /* Do not care about late frames when prerolling
             * TODO avoid decoding of non reference frame
             * (ie all B except for H264 where it depends only on nal_ref_idc) */
            p_sys->i_late_frames = 0;
        }
    }

    if( p_dec->b_frame_drop_allowed && (p_sys->i_late_frames > 0) &&
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
    if( p_dec->b_frame_drop_allowed &&
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
        AVPacket pkt;

        AVFrame *frame = av_frame_alloc();
        if (unlikely(frame == NULL))
        {
            p_dec->b_error = true;
            break;
        }

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

        i_used = avcodec_decode_video2( p_context, frame, &b_gotpicture,
                                        &pkt );
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
                av_frame_free(&frame);
                if( b_drawpicture )
                    msg_Warn( p_dec, "cannot decode one frame (%zu bytes)",
                            p_block->i_buffer );
                break;
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
            av_frame_free(&frame);
            if( i_used == 0 ) break;
            continue;
        }

        /* Compute the PTS */
        mtime_t i_pts = frame->pkt_pts;
        if (i_pts <= VLC_TS_INVALID)
            i_pts = frame->pkt_dts;

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
                p_sys->i_pts += CLOCK_FREQ * (2 + frame->repeat_pict) *
                    p_dec->fmt_in.video.i_frame_rate_base /
                    (2 * p_dec->fmt_in.video.i_frame_rate);
            }
            else if( p_context->time_base.den > 0 )
            {
                int i_tick = p_context->ticks_per_frame;
                if( i_tick <= 0 )
                    i_tick = 1;

                p_sys->i_pts += CLOCK_FREQ * (2 + frame->repeat_pict) *
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

        if( !b_drawpicture || ( !p_sys->p_va && !frame->linesize[0] ) )
        {
            av_frame_free(&frame);
            continue;
        }

        picture_t *p_pic = frame->opaque;
        if( p_pic == NULL )
        {   /* When direct rendering is not used, get_format() and get_buffer()
             * might not be called. The output video format must be set here
             * then picture buffer can be allocated. */
            if (p_sys->p_va == NULL
             && lavc_UpdateVideoFormat(p_dec, p_context, p_context->pix_fmt,
                                       p_context->pix_fmt) == 0)
                p_pic = decoder_GetPicture(p_dec);

            if( !p_pic )
            {
                av_frame_free(&frame);
                break;
            }

            /* Fill picture_t from AVFrame */
            lavc_CopyPicture(p_dec, p_pic, frame);
        }
        else
        {
            if( p_sys->p_va != NULL )
                vlc_va_Extract( p_sys->p_va, p_pic, frame->data[3] );
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

        p_pic->date = i_pts;
        /* Hack to force display of still pictures */
        p_pic->b_force = p_sys->b_first_frame;
        p_pic->i_nb_fields = 2 + frame->repeat_pict;
        p_pic->b_progressive = !frame->interlaced_frame;
        p_pic->b_top_field_first = frame->top_field_first;

        av_frame_free(&frame);

        /* Send decoded frame to vout */
        if (i_pts > VLC_TS_INVALID)
        {
            p_sys->b_first_frame = false;
            return p_pic;
        }
        else
            picture_Release( p_pic );
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
    if( avcodec_is_open( p_sys->p_context ) )
        avcodec_flush_buffers( p_sys->p_context );

    wait_mt( p_sys );

    ffmpeg_CloseCodec( p_dec );

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

static void lavc_ReleaseFrame(void *opaque, uint8_t *data)
{
    picture_t *picture = opaque;

    picture_Release(picture);
    (void) data;
}

static int lavc_va_GetFrame(struct AVCodecContext *ctx, AVFrame *frame,
                            picture_t *pic)
{
    decoder_t *dec = ctx->opaque;
    vlc_va_t *va = dec->p_sys->p_va;

    if (vlc_va_Get(va, pic, &frame->data[0]))
    {
        msg_Err(dec, "hardware acceleration picture allocation failed");
        picture_Release(pic);
        return -1;
    }
    /* data[0] must be non-NULL for libavcodec internal checks.
     * data[3] actually contains the format-specific surface handle. */
    frame->data[3] = frame->data[0];

    void (*release)(void *, uint8_t *) = va->release;
    if (va->release == NULL)
        release = lavc_ReleaseFrame;

    frame->buf[0] = av_buffer_create(frame->data[0], 0, release, pic, 0);
    if (unlikely(frame->buf[0] == NULL))
    {
        release(pic, frame->data[0]);
        return -1;
    }

    frame->opaque = pic;
    assert(frame->data[0] != NULL);
    return 0;
}

static int lavc_dr_GetFrame(struct AVCodecContext *ctx, AVFrame *frame,
                            picture_t *pic)
{
    decoder_t *dec = (decoder_t *)ctx->opaque;
    decoder_sys_t *sys = dec->p_sys;

    if (ctx->pix_fmt == AV_PIX_FMT_PAL8)
        goto error;

    int width = frame->width;
    int height = frame->height;
    int aligns[AV_NUM_DATA_POINTERS];

    avcodec_align_dimensions2(ctx, &width, &height, aligns);

    /* Check that the picture is suitable for libavcodec */
    assert(pic->p[0].i_pitch >= width * pic->p[0].i_pixel_pitch);
    assert(pic->p[0].i_lines >= height);

    for (int i = 0; i < pic->i_planes; i++)
    {
        if (pic->p[i].i_pitch % aligns[i])
        {
            if (!atomic_exchange(&sys->b_dr_failure, true))
                msg_Warn(dec, "plane %d: pitch not aligned (%d%%%d): %s",
                         i, pic->p[i].i_pitch, aligns[i],
                         "disabling direct rendering");
            goto error;
        }
        if (((uintptr_t)pic->p[i].p_pixels) % aligns[i])
        {
            if (!atomic_exchange(&sys->b_dr_failure, true))
                msg_Warn(dec, "plane %d not aligned: %s", i,
                         "disabling direct rendering");
            goto error;
        }
    }

    /* Allocate buffer references and initialize planes */
    assert(pic->i_planes < PICTURE_PLANE_MAX);
    static_assert(PICTURE_PLANE_MAX <= AV_NUM_DATA_POINTERS, "Oops!");

    for (int i = 0; i < pic->i_planes; i++)
    {
        uint8_t *data = pic->p[i].p_pixels;
        int size = pic->p[i].i_pitch * pic->p[i].i_lines;

        frame->data[i] = data;
        frame->linesize[i] = pic->p[i].i_pitch;
        frame->buf[i] = av_buffer_create(data, size, lavc_ReleaseFrame,
                                         pic, 0);
        if (unlikely(frame->buf[i] == NULL))
        {
            while (i > 0)
                av_buffer_unref(&frame->buf[--i]);
            goto error;
        }
        picture_Hold(pic);
    }

    frame->opaque = pic;
    /* The loop above held one reference to the picture for each plane. */
    picture_Release(pic);
    return 0;
error:
    picture_Release(pic);
    return -1;
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
    frame->opaque = NULL;

    wait_mt(sys);
    if (sys->p_va == NULL)
    {
        if (!sys->b_direct_rendering)
        {
            post_mt(sys);
            return avcodec_default_get_buffer2(ctx, frame, flags);
        }

        /* Most unaccelerated decoders do not call get_format(), so we need to
         * update the output video format here. The MT semaphore must be held
         * to protect p_dec->fmt_out. */
        if (lavc_UpdateVideoFormat(dec, ctx, ctx->pix_fmt, ctx->pix_fmt))
        {
            post_mt(sys);
            return -1;
        }
    }
    post_mt(sys);

    pic = decoder_GetPicture(dec);
    if (pic == NULL)
        return -ENOMEM;

    if (sys->p_va != NULL)
        return lavc_va_GetFrame(ctx, frame, pic);

    /* Some codecs set pix_fmt only after the 1st frame has been decoded,
     * so we need to check for direct rendering again. */
    int ret = lavc_dr_GetFrame(ctx, frame, pic);
    if (ret)
        ret = avcodec_default_get_buffer2(ctx, frame, flags);
    return ret;
}

static enum PixelFormat ffmpeg_GetFormat( AVCodecContext *p_context,
                                          const enum PixelFormat *pi_fmt )
{
    decoder_t *p_dec = p_context->opaque;
    decoder_sys_t *p_sys = p_dec->p_sys;
    video_format_t fmt;

    /* Enumerate available formats */
    enum PixelFormat swfmt = avcodec_default_get_format(p_context, pi_fmt);
    bool can_hwaccel = false;

    for( size_t i = 0; pi_fmt[i] != AV_PIX_FMT_NONE; i++ )
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

    /* If the format did not actually change (e.g. seeking), try to reuse the
     * existing output format, and if present, hardware acceleration back-end.
     * This avoids resetting the pipeline downstream. This also avoids
     * needlessly probing for hardware acceleration support. */
    if (p_sys->pix_fmt != AV_PIX_FMT_NONE
     && lavc_GetVideoFormat(p_dec, &fmt, p_context, p_sys->pix_fmt, swfmt) == 0
     && fmt.i_width == p_dec->fmt_out.video.i_width
     && fmt.i_height == p_dec->fmt_out.video.i_height
     && p_context->profile == p_sys->profile
     && p_context->level <= p_sys->level)
    {
        for (size_t i = 0; pi_fmt[i] != AV_PIX_FMT_NONE; i++)
            if (pi_fmt[i] == p_sys->pix_fmt)
            {
                msg_Dbg(p_dec, "reusing decoder output format %d", pi_fmt[i]);
                return p_sys->pix_fmt;
            }
    }

    if (p_sys->p_va != NULL)
    {
        msg_Err(p_dec, "existing hardware acceleration cannot be reused");
        vlc_va_Delete(p_sys->p_va, p_context);
        p_sys->p_va = NULL;
    }

    p_sys->profile = p_context->profile;
    p_sys->level = p_context->level;

    if (!can_hwaccel)
        return swfmt;

#if (LIBAVCODEC_VERSION_MICRO >= 100) /* FFmpeg only */
    if (p_context->active_thread_type)
    {
        msg_Warn(p_dec, "thread type %d: disabling hardware acceleration",
                 p_context->active_thread_type);
        return swfmt;
    }
#endif

    wait_mt(p_sys);

    for( size_t i = 0; pi_fmt[i] != AV_PIX_FMT_NONE; i++ )
    {
        enum PixelFormat hwfmt = pi_fmt[i];

        p_dec->fmt_out.video.i_chroma = vlc_va_GetChroma(hwfmt, swfmt);
        if (p_dec->fmt_out.video.i_chroma == 0)
            continue; /* Unknown brand of hardware acceleration */
        if (p_context->width == 0 || p_context->height == 0)
        {   /* should never happen */
            msg_Err(p_dec, "unspecified video dimensions");
            continue;
        }
        if (lavc_UpdateVideoFormat(p_dec, p_context, hwfmt, swfmt))
            continue; /* Unsupported brand of hardware acceleration */
        post_mt(p_sys);

        picture_t *test_pic = decoder_GetPicture(p_dec);
        assert(!test_pic || test_pic->format.i_chroma == p_dec->fmt_out.video.i_chroma);
        vlc_va_t *va = vlc_va_New(VLC_OBJECT(p_dec), p_context, hwfmt,
                                  &p_dec->fmt_in,
                                  test_pic ? test_pic->p_sys : NULL);
        if (test_pic)
            picture_Release(test_pic);
        if (va == NULL)
        {
            wait_mt(p_sys);
            continue; /* Unsupported codec profile or such */
        }

        if (va->description != NULL)
            msg_Info(p_dec, "Using %s for hardware decoding", va->description);

        p_sys->p_va = va;
        p_sys->pix_fmt = hwfmt;
        p_context->draw_horiz_band = NULL;
        return pi_fmt[i];
    }

    post_mt(p_sys);
    /* Fallback to default behaviour */
    p_sys->pix_fmt = swfmt;
    return swfmt;
}
