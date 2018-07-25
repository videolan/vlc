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
#if (LIBAVUTIL_VERSION_MICRO >= 100 && LIBAVUTIL_VERSION_INT >= AV_VERSION_INT( 55, 16, 101 ) )
#include <libavutil/mastering_display_metadata.h>
#endif

#include "avcodec.h"
#include "va.h"

#include "../codec/cc.h"

/*****************************************************************************
 * decoder_sys_t : decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    AVCodecContext *p_context;
    const AVCodec  *p_codec;

    /* Video decoder specific part */
    date_t  pts;

    /* Closed captions for decoders */
    cc_data_t cc;

    /* for frame skipping algo */
    bool b_hurry_up;
    bool b_show_corrupted;
    bool b_from_preroll;
    enum AVDiscard i_skip_frame;

    /* how many decoded frames are late */
    int     i_late_frames;
    mtime_t i_late_frames_start;
    mtime_t i_last_late_delay;

    /* for direct rendering */
    bool        b_direct_rendering;
    atomic_bool b_dr_failure;

    /* Hack to force display of still pictures */
    bool b_first_frame;


    /* */
    bool palette_sent;

    /* VA API */
    vlc_va_t *p_va;
    enum PixelFormat pix_fmt;
    int profile;
    int level;

    vlc_sem_t sem_mt;
};

static inline void wait_mt(decoder_sys_t *sys)
{
    vlc_sem_wait(&sys->sem_mt);
}

static inline void post_mt(decoder_sys_t *sys)
{
    vlc_sem_post(&sys->sem_mt);
}

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void ffmpeg_InitCodec      ( decoder_t * );
static int lavc_GetFrame(struct AVCodecContext *, AVFrame *, int);
static enum PixelFormat ffmpeg_GetFormat( AVCodecContext *,
                                          const enum PixelFormat * );
static int  DecodeVideo( decoder_t *, block_t * );
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

        /* The libavcodec palette can only be fetched when the first output
         * frame is decoded. Assume that the current chroma is RGB32 while we
         * are waiting for a valid palette. Indeed, fmt_out.video.p_palette
         * doesn't trigger a new vout request, but a new chroma yes. */
        if (pix_fmt == AV_PIX_FMT_PAL8 && !dec->fmt_out.video.p_palette)
            fmt->i_chroma = VLC_CODEC_RGB32;

        avcodec_align_dimensions2(ctx, &width, &height, aligns);
    }
    else /* hardware decoding */
        fmt->i_chroma = vlc_va_GetChroma(pix_fmt, sw_pix_fmt);

    if( width == 0 || height == 0 || width > 8192 || height > 8192 ||
        width < ctx->width || height < ctx->height )
    {
        msg_Err(dec, "Invalid frame size %dx%d vsz %dx%d",
                     width, height, ctx->width, ctx->height );
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
    else if (ctx->framerate.num > 0 && ctx->framerate.den > 0)
    {
        fmt->i_frame_rate = ctx->framerate.num;
        fmt->i_frame_rate_base = ctx->framerate.den;
# if LIBAVCODEC_VERSION_MICRO <  100
        // for some reason libav don't thinkg framerate presents actually same thing as in ffmpeg
        fmt->i_frame_rate_base *= __MAX(ctx->ticks_per_frame, 1);
# endif
    }
    else if (ctx->time_base.num > 0 && ctx->time_base.den > 0)
    {
        fmt->i_frame_rate = ctx->time_base.den;
        fmt->i_frame_rate_base = ctx->time_base.num
                                 * __MAX(ctx->ticks_per_frame, 1);
    }

    /* FIXME we should only set the known values and let the core decide
     * later of fallbacks, but we can't do that with a boolean */
    switch ( ctx->color_range )
    {
    case AVCOL_RANGE_JPEG:
        fmt->b_color_range_full = true;
        break;
    case AVCOL_RANGE_UNSPECIFIED:
        fmt->b_color_range_full = !vlc_fourcc_IsYUV( fmt->i_chroma );
        break;
    case AVCOL_RANGE_MPEG:
    default:
        fmt->b_color_range_full = false;
        break;
    }

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
#if LIBAVUTIL_VERSION_CHECK( 55, 14, 0, 31, 100)
        case AVCOL_TRC_ARIB_STD_B67:
            fmt->transfer = TRANSFER_FUNC_ARIB_B67;
            break;
#endif
#if LIBAVUTIL_VERSION_CHECK( 55, 17, 0, 37, 100)
        case AVCOL_TRC_SMPTE2084:
            fmt->transfer = TRANSFER_FUNC_SMPTE_ST2084;
            break;
        case AVCOL_TRC_SMPTE240M:
            fmt->transfer = TRANSFER_FUNC_SMPTE_240;
            break;
        case AVCOL_TRC_GAMMA28:
            fmt->transfer = TRANSFER_FUNC_BT470_BG;
            break;
#endif
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

    /* always have date in fields/ticks units */
    if(dec->p_sys->pts.i_divider_num)
        date_Change(&dec->p_sys->pts, fmt_out.i_frame_rate *
                                      __MAX(ctx->ticks_per_frame, 1),
                                      fmt_out.i_frame_rate_base);
    else
        date_Init(&dec->p_sys->pts, fmt_out.i_frame_rate *
                                    __MAX(ctx->ticks_per_frame, 1),
                                    fmt_out.i_frame_rate_base);

    fmt_out.p_palette = dec->fmt_out.video.p_palette;
    dec->fmt_out.video.p_palette = NULL;

    es_format_Change(&dec->fmt_out, VIDEO_ES, fmt_out.i_chroma);
    dec->fmt_out.video = fmt_out;
    dec->fmt_out.video.orientation = dec->fmt_in.video.orientation;
    dec->fmt_out.video.projection_mode = dec->fmt_in.video.projection_mode;
    dec->fmt_out.video.multiview_mode = dec->fmt_in.video.multiview_mode;
    dec->fmt_out.video.pose = dec->fmt_in.video.pose;
    if ( dec->fmt_in.video.mastering.max_luminance )
        dec->fmt_out.video.mastering = dec->fmt_in.video.mastering;
    dec->fmt_out.video.lighting = dec->fmt_in.video.lighting;

    return decoder_UpdateVideoFormat(dec);
}

/**
 * Copies a picture from the libavcodec-allocate buffer to a picture_t.
 * This is used when not in direct rendering mode.
 */
static int lavc_CopyPicture(decoder_t *dec, picture_t *pic, AVFrame *frame)
{
    decoder_sys_t *sys = dec->p_sys;

    vlc_fourcc_t fourcc = FindVlcChroma(frame->format);
    if (!fourcc)
    {
        const char *name = av_get_pix_fmt_name(frame->format);

        msg_Err(dec, "Unsupported decoded output format %d (%s)",
                sys->p_context->pix_fmt, (name != NULL) ? name : "unknown");
        return VLC_EGENERIC;
    } else if (fourcc != pic->format.i_chroma
     || frame->width > (int) pic->format.i_width
     || frame->height > (int) pic->format.i_height)
    {
        msg_Warn(dec, "dropping frame because the vout changed");
        return VLC_EGENERIC;
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
    return VLC_SUCCESS;
}

static int OpenVideoCodec( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    AVCodecContext *ctx = p_sys->p_context;
    const AVCodec *codec = p_sys->p_codec;
    int ret;

    if( ctx->extradata_size <= 0 )
    {
        if( codec->id == AV_CODEC_ID_VC1 ||
            codec->id == AV_CODEC_ID_THEORA )
        {
            msg_Warn( p_dec, "waiting for extra data for codec %s",
                      codec->name );
            return 1;
        }
    }

    ctx->width  = p_dec->fmt_in.video.i_visible_width;
    ctx->height = p_dec->fmt_in.video.i_visible_height;

    ctx->coded_width = p_dec->fmt_in.video.i_width;
    ctx->coded_height = p_dec->fmt_in.video.i_height;

    ctx->bits_per_coded_sample = p_dec->fmt_in.video.i_bits_per_pixel;
    p_sys->pix_fmt = AV_PIX_FMT_NONE;
    p_sys->profile = -1;
    p_sys->level = -1;
    cc_Init( &p_sys->cc );

    set_video_color_settings( &p_dec->fmt_in.video, ctx );
    if( p_dec->fmt_in.video.i_frame_rate_base &&
        p_dec->fmt_in.video.i_frame_rate &&
        (double) p_dec->fmt_in.video.i_frame_rate /
                 p_dec->fmt_in.video.i_frame_rate_base < 6 )
    {
        ctx->flags |= AV_CODEC_FLAG_LOW_DELAY;
    }

    post_mt( p_sys );
    ret = ffmpeg_OpenCodec( p_dec, ctx, codec );
    wait_mt( p_sys );
    if( ret < 0 )
        return ret;

    switch( ctx->active_thread_type )
    {
        case FF_THREAD_FRAME:
            msg_Dbg( p_dec, "using frame thread mode with %d threads",
                     ctx->thread_count );
            break;
        case FF_THREAD_SLICE:
            msg_Dbg( p_dec, "using slice thread mode with %d threads",
                     ctx->thread_count );
            break;
        case 0:
            if( ctx->thread_count > 1 )
                msg_Warn( p_dec, "failed to enable threaded decoding" );
            break;
        default:
            msg_Warn( p_dec, "using unknown thread mode with %d threads",
                      ctx->thread_count );
            break;
    }
    return 0;
}

/*****************************************************************************
 * InitVideo: initialize the video decoder
 *****************************************************************************
 * the ffmpeg codec will be opened, some memory allocated. The vout is not yet
 * opened (done after the first decoded frame).
 *****************************************************************************/
int InitVideoDec( vlc_object_t *obj )
{
    decoder_t *p_dec = (decoder_t *)obj;
    const AVCodec *p_codec;
    AVCodecContext *p_context = ffmpeg_AllocContext( p_dec, &p_codec );
    if( p_context == NULL )
        return VLC_EGENERIC;

    int i_val;

    /* Allocate the memory needed to store the decoder's structure */
    decoder_sys_t *p_sys = calloc( 1, sizeof(*p_sys) );
    if( unlikely(p_sys == NULL) )
    {
        avcodec_free_context( &p_context );
        return VLC_ENOMEM;
    }

    p_dec->p_sys = p_sys;
    p_sys->p_context = p_context;
    p_sys->p_codec = p_codec;
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
        p_context->flags |= AV_CODEC_FLAG_GRAY;

    /* ***** Output always the frames ***** */
    p_context->flags |= AV_CODEC_FLAG_OUTPUT_CORRUPT;

    i_val = var_CreateGetInteger( p_dec, "avcodec-skiploopfilter" );
    if( i_val >= 4 ) p_context->skip_loop_filter = AVDISCARD_ALL;
    else if( i_val == 3 ) p_context->skip_loop_filter = AVDISCARD_NONKEY;
    else if( i_val == 2 ) p_context->skip_loop_filter = AVDISCARD_BIDIR;
    else if( i_val == 1 ) p_context->skip_loop_filter = AVDISCARD_NONREF;
    else p_context->skip_loop_filter = AVDISCARD_DEFAULT;

    if( var_CreateGetBool( p_dec, "avcodec-fast" ) )
        p_context->flags2 |= AV_CODEC_FLAG2_FAST;

    /* ***** libavcodec frame skipping ***** */
    p_sys->b_hurry_up = var_CreateGetBool( p_dec, "avcodec-hurry-up" );
    p_sys->b_show_corrupted = var_CreateGetBool( p_dec, "avcodec-corrupted" );

    i_val = var_CreateGetInteger( p_dec, "avcodec-skip-frame" );
    if( i_val >= 4 ) p_sys->i_skip_frame = AVDISCARD_ALL;
    else if( i_val == 3 ) p_sys->i_skip_frame = AVDISCARD_NONKEY;
    else if( i_val == 2 ) p_sys->i_skip_frame = AVDISCARD_BIDIR;
    else if( i_val == 1 ) p_sys->i_skip_frame = AVDISCARD_NONREF;
    else if( i_val == -1 ) p_sys->i_skip_frame = AVDISCARD_NONE;
    else p_sys->i_skip_frame = AVDISCARD_DEFAULT;
    p_context->skip_frame = p_sys->i_skip_frame;

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
       (p_codec->capabilities & AV_CODEC_CAP_DR1) &&
        /* No idea why ... but this fixes flickering on some TSCC streams */
        p_sys->p_codec->id != AV_CODEC_ID_TSCC &&
        p_sys->p_codec->id != AV_CODEC_ID_CSCD &&
        p_sys->p_codec->id != AV_CODEC_ID_CINEPAK )
    {
        /* Some codecs set pix_fmt only after the 1st frame has been decoded,
         * so we need to do another check in ffmpeg_GetFrameBuf() */
        p_sys->b_direct_rendering = true;
    }

    p_context->get_format = ffmpeg_GetFormat;
    /* Always use our get_buffer wrapper so we can calculate the
     * PTS correctly */
    p_context->get_buffer2 = lavc_GetFrame;
    p_context->opaque = p_dec;

    int i_thread_count = var_InheritInteger( p_dec, "avcodec-threads" );
    if( i_thread_count <= 0 )
    {
        i_thread_count = vlc_GetCPUCount();
        if( i_thread_count > 1 )
            i_thread_count++;

        //FIXME: take in count the decoding time
#if VLC_WINSTORE_APP
        i_thread_count = __MIN( i_thread_count, 6 );
#else
        i_thread_count = __MIN( i_thread_count, p_codec->id == AV_CODEC_ID_HEVC ? 10 : 6 );
#endif
    }
    i_thread_count = __MIN( i_thread_count, p_codec->id == AV_CODEC_ID_HEVC ? 32 : 16 );
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

    /* ***** misc init ***** */
    date_Init(&p_sys->pts, 1, 30001);
    date_Set(&p_sys->pts, VLC_TS_INVALID);
    p_sys->b_first_frame = true;
    p_sys->i_late_frames = 0;
    p_sys->b_from_preroll = false;

    /* Set output properties */
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
        avcodec_free_context( &p_context );
        return VLC_EGENERIC;
    }

    p_dec->pf_decode = DecodeVideo;
    p_dec->pf_flush  = Flush;

    /* XXX: Writing input format makes little sense. */
    if( p_context->profile != FF_PROFILE_UNKNOWN )
        p_dec->fmt_in.i_profile = p_context->profile;
    if( p_context->level != FF_LEVEL_UNKNOWN )
        p_dec->fmt_in.i_level = p_context->level;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Flush:
 *****************************************************************************/
static void Flush( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    AVCodecContext *p_context = p_sys->p_context;

    date_Set(&p_sys->pts, VLC_TS_INVALID); /* To make sure we recover properly */
    p_sys->i_late_frames = 0;
    cc_Flush( &p_sys->cc );

    /* Abort pictures in order to unblock all avcodec workers threads waiting
     * for a picture. This will avoid a deadlock between avcodec_flush_buffers
     * and workers threads */
    decoder_AbortPictures( p_dec, true );

    post_mt( p_sys );
    /* do not flush buffers if codec hasn't been opened (theora/vorbis/VC1) */
    if( avcodec_is_open( p_context ) )
        avcodec_flush_buffers( p_context );
    wait_mt( p_sys );

    /* Reset cancel state to false */
    decoder_AbortPictures( p_dec, false );
}

static bool check_block_validity( decoder_sys_t *p_sys, block_t *block )
{
    if( !block)
        return true;

    if( block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) )
    {
        date_Set( &p_sys->pts, VLC_TS_INVALID ); /* To make sure we recover properly */
        cc_Flush( &p_sys->cc );

        p_sys->i_late_frames = 0;
        if( block->i_flags & BLOCK_FLAG_CORRUPTED )
        {
            block_Release( block );
            return false;
        }
    }
    return true;
}

static bool check_block_being_late( decoder_sys_t *p_sys, block_t *block, mtime_t current_time)
{
    if( !block )
        return false;
    if( block->i_flags & BLOCK_FLAG_PREROLL )
    {
        /* Do not care about late frames when prerolling
         * TODO avoid decoding of non reference frame
         * (ie all B except for H264 where it depends only on nal_ref_idc) */
        p_sys->i_late_frames = 0;
        p_sys->b_from_preroll = true;
        p_sys->i_last_late_delay = INT64_MAX;
    }

    if( p_sys->i_late_frames <= 0 )
        return false;

    if( current_time - p_sys->i_late_frames_start > (5*CLOCK_FREQ))
    {
        date_Set( &p_sys->pts, VLC_TS_INVALID ); /* To make sure we recover properly */
        block_Release( block );
        p_sys->i_late_frames--;
        return true;
    }
    return false;
}

static bool check_frame_should_be_dropped( decoder_sys_t *p_sys, AVCodecContext *p_context, bool *b_need_output_picture )
{
    if( p_sys->i_late_frames <= 4)
        return false;

    *b_need_output_picture = false;
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
        return true;
    }
    return false;
}

static void interpolate_next_pts( decoder_t *p_dec, AVFrame *frame )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    AVCodecContext *p_context = p_sys->p_context;

    if( date_Get( &p_sys->pts ) == VLC_TS_INVALID ||
        p_sys->pts.i_divider_num == 0 )
        return;

    int i_tick = p_context->ticks_per_frame;
    if( i_tick <= 0 )
        i_tick = 1;

    /* interpolate the next PTS */
    date_Increment( &p_sys->pts, i_tick + frame->repeat_pict );
}

static void update_late_frame_count( decoder_t *p_dec, block_t *p_block, mtime_t current_time, mtime_t i_pts )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
   /* Update frame late count (except when doing preroll) */
   mtime_t i_display_date = VLC_TS_INVALID;
   if( !p_block || !(p_block->i_flags & BLOCK_FLAG_PREROLL) )
       i_display_date = decoder_GetDisplayDate( p_dec, i_pts );

   if( i_display_date > VLC_TS_INVALID && i_display_date <= current_time )
   {
       /* Out of preroll, consider only late frames on rising delay */
       if( p_sys->b_from_preroll )
       {
           if( p_sys->i_last_late_delay > current_time - i_display_date )
           {
               p_sys->i_last_late_delay = current_time - i_display_date;
               return;
           }
           p_sys->b_from_preroll = false;
       }

       p_sys->i_late_frames++;
       if( p_sys->i_late_frames == 1 )
           p_sys->i_late_frames_start = current_time;

   }
   else
   {
       p_sys->i_late_frames = 0;
   }
}


static int DecodeSidedata( decoder_t *p_dec, const AVFrame *frame, picture_t *p_pic )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    bool format_changed = false;

#if (LIBAVUTIL_VERSION_MICRO >= 100 && LIBAVUTIL_VERSION_INT >= AV_VERSION_INT( 55, 16, 101 ) )
#define FROM_AVRAT(default_factor, avrat) \
(uint64_t)(default_factor) * (avrat).num / (avrat).den
    const AVFrameSideData *metadata =
            av_frame_get_side_data( frame,
                                    AV_FRAME_DATA_MASTERING_DISPLAY_METADATA );
    if ( metadata )
    {
        const AVMasteringDisplayMetadata *hdr_meta =
                (const AVMasteringDisplayMetadata *) metadata->data;
        if ( hdr_meta->has_luminance )
        {
#define ST2086_LUMA_FACTOR 10000
            p_pic->format.mastering.max_luminance =
                    FROM_AVRAT(ST2086_LUMA_FACTOR, hdr_meta->max_luminance);
            p_pic->format.mastering.min_luminance =
                    FROM_AVRAT(ST2086_LUMA_FACTOR, hdr_meta->min_luminance);
        }
        if ( hdr_meta->has_primaries )
        {
#define ST2086_RED   2
#define ST2086_GREEN 0
#define ST2086_BLUE  1
#define LAV_RED    0
#define LAV_GREEN  1
#define LAV_BLUE   2
#define ST2086_PRIM_FACTOR 50000
            p_pic->format.mastering.primaries[ST2086_RED*2   + 0] =
                    FROM_AVRAT(ST2086_PRIM_FACTOR, hdr_meta->display_primaries[LAV_RED][0]);
            p_pic->format.mastering.primaries[ST2086_RED*2   + 1] =
                    FROM_AVRAT(ST2086_PRIM_FACTOR, hdr_meta->display_primaries[LAV_RED][1]);
            p_pic->format.mastering.primaries[ST2086_GREEN*2 + 0] =
                    FROM_AVRAT(ST2086_PRIM_FACTOR, hdr_meta->display_primaries[LAV_GREEN][0]);
            p_pic->format.mastering.primaries[ST2086_GREEN*2 + 1] =
                    FROM_AVRAT(ST2086_PRIM_FACTOR, hdr_meta->display_primaries[LAV_GREEN][1]);
            p_pic->format.mastering.primaries[ST2086_BLUE*2  + 0] =
                    FROM_AVRAT(ST2086_PRIM_FACTOR, hdr_meta->display_primaries[LAV_BLUE][0]);
            p_pic->format.mastering.primaries[ST2086_BLUE*2  + 1] =
                    FROM_AVRAT(ST2086_PRIM_FACTOR, hdr_meta->display_primaries[LAV_BLUE][1]);
            p_pic->format.mastering.white_point[0] =
                    FROM_AVRAT(ST2086_PRIM_FACTOR, hdr_meta->white_point[0]);
            p_pic->format.mastering.white_point[1] =
                    FROM_AVRAT(ST2086_PRIM_FACTOR, hdr_meta->white_point[1]);
        }

        if ( memcmp( &p_dec->fmt_out.video.mastering,
                     &p_pic->format.mastering,
                     sizeof(p_pic->format.mastering) ) )
        {
            p_dec->fmt_out.video.mastering = p_pic->format.mastering;
            format_changed = true;
        }
#undef FROM_AVRAT
    }
#endif
#if (LIBAVUTIL_VERSION_MICRO >= 100 && LIBAVUTIL_VERSION_INT >= AV_VERSION_INT( 55, 60, 100 ) )
    const AVFrameSideData *metadata_lt =
            av_frame_get_side_data( frame,
                                    AV_FRAME_DATA_CONTENT_LIGHT_LEVEL );
    if ( metadata_lt )
    {
        const AVContentLightMetadata *light_meta =
                (const AVContentLightMetadata *) metadata_lt->data;
        p_pic->format.lighting.MaxCLL = light_meta->MaxCLL;
        p_pic->format.lighting.MaxFALL = light_meta->MaxFALL;
        if ( memcmp( &p_dec->fmt_out.video.lighting,
                     &p_pic->format.lighting,
                     sizeof(p_pic->format.lighting) ) )
        {
            p_dec->fmt_out.video.lighting  = p_pic->format.lighting;
            format_changed = true;
        }
    }
#endif

    if (format_changed && decoder_UpdateVideoFormat( p_dec ))
        return -1;

    const AVFrameSideData *p_avcc = av_frame_get_side_data( frame, AV_FRAME_DATA_A53_CC );
    if( p_avcc )
    {
        cc_Extract( &p_sys->cc, CC_PAYLOAD_RAW, true, p_avcc->data, p_avcc->size );
        if( p_sys->cc.b_reorder || p_sys->cc.i_data )
        {
            block_t *p_cc = block_Alloc( p_sys->cc.i_data );
            if( p_cc )
            {
                memcpy( p_cc->p_buffer, p_sys->cc.p_data, p_sys->cc.i_data );
                if( p_sys->cc.b_reorder )
                    p_cc->i_dts = p_cc->i_pts = p_pic->date;
                else
                    p_cc->i_pts = p_cc->i_dts;
                decoder_cc_desc_t desc;
                desc.i_608_channels = p_sys->cc.i_608channels;
                desc.i_708_channels = p_sys->cc.i_708channels;
                desc.i_reorder_depth = 4;
                decoder_QueueCc( p_dec, p_cc, &desc );
            }
            cc_Flush( &p_sys->cc );
        }
    }
    return 0;
}

/*****************************************************************************
 * DecodeBlock: Called to decode one or more frames
 *****************************************************************************/
static picture_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block, bool *error )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    AVCodecContext *p_context = p_sys->p_context;
    /* Boolean if we assume that we should get valid pic as result */
    bool b_need_output_picture = true;

    /* Boolean for END_OF_SEQUENCE */
    bool eos_spotted = false;


    block_t *p_block;
    mtime_t current_time;

    if( !p_context->extradata_size && p_dec->fmt_in.i_extra )
    {
        ffmpeg_InitCodec( p_dec );
        if( !avcodec_is_open( p_context ) )
            OpenVideoCodec( p_dec );
    }

    p_block = pp_block ? *pp_block : NULL;
    if(!p_block && !(p_sys->p_codec->capabilities & AV_CODEC_CAP_DELAY) )
        return NULL;

    if( !avcodec_is_open( p_context ) )
    {
        if( p_block )
            block_Release( p_block );
        return NULL;
    }

    if( !check_block_validity( p_sys, p_block ) )
        return NULL;

    current_time = mdate();
    if( p_dec->b_frame_drop_allowed &&  check_block_being_late( p_sys, p_block, current_time) )
    {
        msg_Err( p_dec, "more than 5 seconds of late video -> "
                 "dropping frame (computer too slow ?)" );
        return NULL;
    }


    /* A good idea could be to decode all I pictures and see for the other */

    /* Defaults that if we aren't in prerolling, we want output picture
       same for if we are flushing (p_block==NULL) */
    if( !p_block || !(p_block->i_flags & BLOCK_FLAG_PREROLL) )
        b_need_output_picture = true;
    else
        b_need_output_picture = false;

    /* Change skip_frame config only if hurry_up is enabled */
    if( p_sys->b_hurry_up )
    {
        p_context->skip_frame = p_sys->i_skip_frame;

        /* Check also if we should/can drop the block and move to next block
            as trying to catchup the speed*/
        if( p_dec->b_frame_drop_allowed &&
            check_frame_should_be_dropped( p_sys, p_context, &b_need_output_picture ) )
        {
            if( p_block )
                block_Release( p_block );
            msg_Warn( p_dec, "More than 11 late frames, dropping frame" );
            return NULL;
        }
    }
    if( !b_need_output_picture )
    {
        p_context->skip_frame = __MAX( p_context->skip_frame,
                                              AVDISCARD_NONREF );
    }

    /*
     * Do the actual decoding now */

    /* Don't forget that libavcodec requires a little more bytes
     * that the real frame size */
    if( p_block && p_block->i_buffer > 0 )
    {
        eos_spotted = ( p_block->i_flags & BLOCK_FLAG_END_OF_SEQUENCE ) != 0;

        p_block = block_Realloc( p_block, 0,
                            p_block->i_buffer + FF_INPUT_BUFFER_PADDING_SIZE );
        if( !p_block )
            return NULL;
        p_block->i_buffer -= FF_INPUT_BUFFER_PADDING_SIZE;
        *pp_block = p_block;
        memset( p_block->p_buffer + p_block->i_buffer, 0,
                FF_INPUT_BUFFER_PADDING_SIZE );
    }

    while( !p_block || p_block->i_buffer > 0 || eos_spotted )
    {
        int i_used;
        AVPacket pkt;

        post_mt( p_sys );

        av_init_packet( &pkt );
        if( p_block && p_block->i_buffer > 0 )
        {
            pkt.data = p_block->p_buffer;
            pkt.size = p_block->i_buffer;
            pkt.pts = p_block->i_pts > VLC_TS_INVALID ? p_block->i_pts : AV_NOPTS_VALUE;
            pkt.dts = p_block->i_dts > VLC_TS_INVALID ? p_block->i_dts : AV_NOPTS_VALUE;
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

#if LIBAVCODEC_VERSION_CHECK( 57, 0, 0xFFFFFFFFU, 64, 101 )
        if( !b_need_output_picture )
            pkt.flags |= AV_PKT_FLAG_DISCARD;
#endif

        int ret = avcodec_send_packet(p_context, &pkt);
        if( ret != 0 && ret != AVERROR(EAGAIN) )
        {
            if (ret == AVERROR(ENOMEM) || ret == AVERROR(EINVAL))
            {
                msg_Err(p_dec, "avcodec_send_packet critical error");
                *error = true;
            }
            av_packet_unref( &pkt );
            break;
        }
        i_used = ret != AVERROR(EAGAIN) ? pkt.size : 0;
        av_packet_unref( &pkt );

        AVFrame *frame = av_frame_alloc();
        if (unlikely(frame == NULL))
        {
            *error = true;
            break;
        }

        ret = avcodec_receive_frame(p_context, frame);
        if( ret != 0 && ret != AVERROR(EAGAIN) )
        {
            if (ret == AVERROR(ENOMEM) || ret == AVERROR(EINVAL))
            {
                msg_Err(p_dec, "avcodec_receive_frame critical error");
                *error = true;
            }
            av_frame_free(&frame);
            /* After draining, we need to reset decoder with a flush */
            if( ret == AVERROR_EOF )
                avcodec_flush_buffers( p_sys->p_context );
            break;
        }
        bool not_received_frame = ret;

        wait_mt( p_sys );

        if( eos_spotted )
            p_sys->b_first_frame = true;

        if( p_block )
        {
            if( p_block->i_buffer <= 0 )
                eos_spotted = false;

            /* Consumed bytes */
            p_block->p_buffer += i_used;
            p_block->i_buffer -= i_used;
        }

        /* Nothing to display */
        if( not_received_frame )
        {
            av_frame_free(&frame);
            if( i_used == 0 ) break;
            continue;
        }

        /* Compute the PTS */
#ifdef FF_API_PKT_PTS
        mtime_t i_pts = frame->pts;
#else
        mtime_t i_pts = frame->pkt_pts;
#endif
        if (i_pts == AV_NOPTS_VALUE )
            i_pts = frame->pkt_dts;

        if( i_pts == AV_NOPTS_VALUE )
            i_pts = date_Get( &p_sys->pts );

        /* Interpolate the next PTS */
        if( i_pts > VLC_TS_INVALID )
            date_Set( &p_sys->pts, i_pts );

        interpolate_next_pts( p_dec, frame );

        update_late_frame_count( p_dec, p_block, current_time, i_pts);

        if( ( !p_sys->p_va && !frame->linesize[0] ) ||
           ( p_dec->b_frame_drop_allowed && (frame->flags & AV_FRAME_FLAG_CORRUPT) &&
             !p_sys->b_show_corrupted ) )
        {
            av_frame_free(&frame);
            continue;
        }

#if !LIBAVCODEC_VERSION_CHECK( 57, 0, 0xFFFFFFFFU, 64, 101 )
        if( !b_need_output_picture )
        {
            av_frame_free(&frame);
            continue;
        }
#endif

        if( p_context->pix_fmt == AV_PIX_FMT_PAL8
         && !p_dec->fmt_out.video.p_palette )
        {
            /* See AV_PIX_FMT_PAL8 comment in avc_GetVideoFormat(): update the
             * fmt_out palette and change the fmt_out chroma to request a new
             * vout */
            assert( p_dec->fmt_out.video.i_chroma != VLC_CODEC_RGBP );

            video_palette_t *p_palette;
            p_palette = p_dec->fmt_out.video.p_palette
                      = malloc( sizeof(video_palette_t) );
            if( !p_palette )
            {
                *error = true;
                av_frame_free(&frame);
                break;
            }
            static_assert( sizeof(p_palette->palette) == AVPALETTE_SIZE,
                           "Palette size mismatch between vlc and libavutil" );
            assert( frame->data[1] != NULL );
            memcpy( p_palette->palette, frame->data[1], AVPALETTE_SIZE );
            p_palette->i_entries = AVPALETTE_COUNT;
            p_dec->fmt_out.video.i_chroma = VLC_CODEC_RGBP;
            if( decoder_UpdateVideoFormat( p_dec ) )
            {
                av_frame_free(&frame);
                continue;
            }
        }

        picture_t *p_pic = frame->opaque;
        if( p_pic == NULL )
        {   /* When direct rendering is not used, get_format() and get_buffer()
             * might not be called. The output video format must be set here
             * then picture buffer can be allocated. */
            if (p_sys->p_va == NULL
             && lavc_UpdateVideoFormat(p_dec, p_context, p_context->pix_fmt,
                                       p_context->pix_fmt) == 0)
                p_pic = decoder_NewPicture(p_dec);

            if( !p_pic )
            {
                av_frame_free(&frame);
                break;
            }

            /* Fill picture_t from AVFrame */
            if( lavc_CopyPicture( p_dec, p_pic, frame ) != VLC_SUCCESS )
            {
                av_frame_free(&frame);
                picture_Release( p_pic );
                break;
            }
        }
        else
        {
            /* Some codecs can return the same frame multiple times. By the
             * time that the same frame is returned a second time, it will be
             * too late to clone the underlying picture. So clone proactively.
             * A single picture CANNOT be queued multiple times.
             */
            p_pic = picture_Clone( p_pic );
            if( unlikely(p_pic == NULL) )
            {
                av_frame_free(&frame);
                break;
            }
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

        if (DecodeSidedata(p_dec, frame, p_pic))
            i_pts = VLC_TS_INVALID;

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

static int DecodeVideo( decoder_t *p_dec, block_t *p_block )
{
    block_t **pp_block = p_block ? &p_block : NULL;
    picture_t *p_pic;
    bool error = false;
    while( ( p_pic = DecodeBlock( p_dec, pp_block, &error ) ) != NULL )
        decoder_QueueVideo( p_dec, p_pic );
    return error ? VLCDEC_ECRITICAL : VLCDEC_SUCCESS;
}

/*****************************************************************************
 * EndVideo: decoder destruction
 *****************************************************************************
 * This function is called when the thread ends after a successful
 * initialization.
 *****************************************************************************/
void EndVideoDec( vlc_object_t *obj )
{
    decoder_t *p_dec = (decoder_t *)obj;
    decoder_sys_t *p_sys = p_dec->p_sys;
    AVCodecContext *ctx = p_sys->p_context;
    void *hwaccel_context;

    post_mt( p_sys );

    /* do not flush buffers if codec hasn't been opened (theora/vorbis/VC1) */
    if( avcodec_is_open( ctx ) )
        avcodec_flush_buffers( ctx );

    wait_mt( p_sys );

    cc_Flush( &p_sys->cc );

    hwaccel_context = ctx->hwaccel_context;
    avcodec_free_context( &ctx );

    if( p_sys->p_va )
        vlc_va_Delete( p_sys->p_va, &hwaccel_context );

    vlc_sem_destroy( &p_sys->sem_mt );
    free( p_sys );
}

/*****************************************************************************
 * ffmpeg_InitCodec: setup codec extra initialization data for ffmpeg
 *****************************************************************************/
static void ffmpeg_InitCodec( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    size_t i_size = p_dec->fmt_in.i_extra;

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
                uint_fast32_t atom_size = GetDWBE( psz );
                if( atom_size <= 1 )
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

                psz += atom_size;
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
    (void) data;
    picture_t *picture = opaque;

    picture_Release(picture);
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
    assert(frame->data[0] != NULL);
    /* data[0] must be non-NULL for libavcodec internal checks.
     * data[3] actually contains the format-specific surface handle. */
    frame->data[3] = frame->data[0];

    frame->buf[0] = av_buffer_create(frame->data[0], 0, lavc_ReleaseFrame, pic, 0);
    if (unlikely(frame->buf[0] == NULL))
    {
        lavc_ReleaseFrame(pic, frame->data[0]);
        return -1;
    }

    frame->opaque = pic;
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
                msg_Warn(dec, "plane %d: pitch not aligned (%d%%%d): disabling direct rendering",
                         i, pic->p[i].i_pitch, aligns[i]);
            goto error;
        }
        if (((uintptr_t)pic->p[i].p_pixels) % aligns[i])
        {
            if (!atomic_exchange(&sys->b_dr_failure, true))
                msg_Warn(dec, "plane %d not aligned: disabling direct rendering", i);
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

    pic = decoder_NewPicture(dec);
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

    for (size_t i = 0; pi_fmt[i] != AV_PIX_FMT_NONE; i++)
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
        vlc_va_Delete(p_sys->p_va, &p_context->hwaccel_context);
        p_sys->p_va = NULL;
    }

    p_sys->profile = p_context->profile;
    p_sys->level = p_context->level;

    if (!can_hwaccel)
        return swfmt;

#if (LIBAVCODEC_VERSION_MICRO >= 100) \
  && (LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 83, 101))
    if (p_context->active_thread_type)
    {
        msg_Warn(p_dec, "thread type %d: disabling hardware acceleration",
                 p_context->active_thread_type);
        return swfmt;
    }
#endif

    wait_mt(p_sys);

    static const enum PixelFormat hwfmts[] =
    {
#ifdef _WIN32
#if LIBAVUTIL_VERSION_CHECK(54, 13, 1, 24, 100)
        AV_PIX_FMT_D3D11VA_VLD,
#endif
        AV_PIX_FMT_DXVA2_VLD,
#endif
        AV_PIX_FMT_VAAPI_VLD,
#if (LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(52, 4, 0))
        AV_PIX_FMT_VDPAU,
#endif
        AV_PIX_FMT_NONE,
    };

    for( size_t i = 0; hwfmts[i] != AV_PIX_FMT_NONE; i++ )
    {
        enum PixelFormat hwfmt = AV_PIX_FMT_NONE;
        for( size_t j = 0; hwfmt == AV_PIX_FMT_NONE && pi_fmt[j] != AV_PIX_FMT_NONE; j++ )
            if( hwfmts[i] == pi_fmt[j] )
                hwfmt = hwfmts[i];

        if( hwfmt == AV_PIX_FMT_NONE )
            continue;

        p_dec->fmt_out.video.i_chroma = vlc_va_GetChroma(hwfmt, swfmt);
        if (p_dec->fmt_out.video.i_chroma == 0)
            continue; /* Unknown brand of hardware acceleration */
        if (p_context->width == 0 || p_context->height == 0)
        {   /* should never happen */
            msg_Err(p_dec, "unspecified video dimensions");
            continue;
        }
        const AVPixFmtDescriptor *dsc = av_pix_fmt_desc_get(hwfmt);
        msg_Dbg(p_dec, "trying format %s", dsc ? dsc->name : "unknown");
        if (lavc_UpdateVideoFormat(p_dec, p_context, hwfmt, swfmt))
            continue; /* Unsupported brand of hardware acceleration */
        post_mt(p_sys);

        picture_t *test_pic = decoder_NewPicture(p_dec);
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
        return hwfmt;
    }

    post_mt(p_sys);
    /* Fallback to default behaviour */
    p_sys->pix_fmt = swfmt;
    return swfmt;
}
